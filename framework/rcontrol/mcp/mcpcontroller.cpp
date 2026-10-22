/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore/Audacity CLA applies
 *
 * Copyright (C) 2026 MuseScore/Audacity and others
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "mcpcontroller.h"

#include "mcpserver.h"

#include <cstdio>

#include "global/serialization/json.h"

#include "log.h"
#include "thirdparty/kors_logger/src/log_base.h"

using namespace muse;
using namespace muse::rcontrol::mcp;
using namespace muse::rcommand;

McpController::McpController(const modularity::ContextPtr& iocCtx)
    : Contextable(iocCtx)
{
}

McpController::~McpController()
{
    stop();
}

//! NOTE MCP tool names: letters, digits, '_' and '-', at most 64 characters.
//! "trackedit/clip/change-color" becomes "trackedit_clip_change-color"; a longer
//! path (effects carry a plugin id) keeps a readable prefix and a hash of the
//! whole path, so names stay unique.
static std::string commandToToolName(const Command& command)
{
    static constexpr size_t MAX_LEN = 64;
    static constexpr size_t HASH_LEN = 8;

    const std::string& path = command.path();
    std::string name;
    name.reserve(path.size());
    for (char c : path) {
        const bool allowed = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-';
        name += allowed ? c : '_';
    }

    if (name.size() > MAX_LEN) {
        uint32_t hash = 2166136261u; // FNV-1a over the original path
        for (unsigned char c : path) {
            hash = (hash ^ c) * 16777619u;
        }
        char hex[HASH_LEN + 1];
        std::snprintf(hex, sizeof(hex), "%08x", hash);
        name = name.substr(0, MAX_LEN - HASH_LEN - 1) + "_" + hex;
    }
    return name;
}

static mcp::DataType toMcpType(rcommand::DataType type)
{
    switch (type) {
    case rcommand::DataType::String: return mcp::DataType::String;
    case rcommand::DataType::Integer: return mcp::DataType::Integer;
    case rcommand::DataType::Float: return mcp::DataType::Float;
    case rcommand::DataType::Boolean: return mcp::DataType::Boolean;
    case rcommand::DataType::Object: return mcp::DataType::Object;
    case rcommand::DataType::Array: return mcp::DataType::Array;
    case rcommand::DataType::Null: return mcp::DataType::Null;
    case rcommand::DataType::Undefined: break;
    }
    return mcp::DataType::Undefined;
}

static muse::JsonValue toJson(const muse::Val& val)
{
    switch (val.type()) {
    case muse::Val::Type::Undefined: return muse::JsonValue();
    case muse::Val::Type::Bool: return muse::JsonValue(val.toBool());
    case muse::Val::Type::Int: return muse::JsonValue(val.toInt());
    case muse::Val::Type::Int64:
    case muse::Val::Type::Double: return muse::JsonValue(val.toDouble());
    case muse::Val::Type::String: return muse::JsonValue(val.toString());
    case muse::Val::Type::List: {
        muse::JsonArray arr;
        for (const muse::Val& v : val.toList()) {
            arr << toJson(v);
        }
        muse::JsonValue jv;
        jv = arr;
        return jv;
    }
    case muse::Val::Type::Map: {
        muse::JsonObject obj;
        for (const auto& [key, v] : val.toMap()) {
            obj.set(key, toJson(v));
        }
        muse::JsonValue jv;
        jv = obj;
        return jv;
    }
#ifndef NO_QT_SUPPORT
    case muse::Val::Type::Color: return muse::JsonValue(val.toString());
#endif
    }
    return muse::JsonValue();
}

//! NOTE Arguments arrive as JSON; the command declares the type it expects for each.
static Ret toParams(const CommandInfo& info, const muse::JsonObject& args, Params& params)
{
    for (const std::string& key : args.keys()) {
        auto it = info.inputSchema.args.find(key);
        if (it == info.inputSchema.args.end()) {
            return make_ret(Ret::Code::BadArgs, "unknown argument: " + key);
        }

        const muse::JsonValue v = args.value(key);
        switch (it->second.type) {
        case rcommand::DataType::String:
            params[key] = muse::Val(v.toStdString());
            break;
        case rcommand::DataType::Integer:
            params[key] = muse::Val(v.toInt());
            break;
        case rcommand::DataType::Float:
            params[key] = muse::Val(v.toDouble());
            break;
        case rcommand::DataType::Boolean:
            params[key] = muse::Val(v.toBool());
            break;
        default:
            return make_ret(Ret::Code::BadArgs, "argument of unsupported type: " + key);
        }
    }
    return make_ok();
}

static ToolResult toToolResult(const Response& response)
{
    ToolResult result;
    result.isError = !response.ret;
    result.text = response.ret.text();
    if (result.isError && result.text.empty()) {
        result.text = "failed with code " + std::to_string(response.ret.code());
    }

    //! NOTE Commands that return data put a Val in Response::data
    if (response.data.has_value() && response.data.type() == typeid(muse::Val)) {
        const muse::JsonValue json = toJson(std::any_cast<muse::Val>(response.data));
        if (json.isObject()) {
            result.structuredContent = json;
            const ByteArray ba = JsonDocument(json.toObject()).toJson(JsonDocument::Format::Compact);
            result.text = std::string(ba.constChar(), ba.size());
        }
    }
    return result;
}

bool McpController::start()
{
    if (m_mcpServer) {
        return true;
    }

    m_mcpServer = std::make_unique<McpServer>(application()->version().toStdString());

    m_mcpServer->onToolsListRequest([this](const McpServer::ToolsListResultHandler& onResult) {
        std::vector<Tool> tools = makeToolsList();
        onResult(tools);
    });

    m_mcpServer->onToolsCallRequest([this](const std::string& name,
                                           const muse::JsonObject& args,
                                           const McpServer::ToolsCallResultHandler& onResult)
    {
        LOGDA() << "Tools call: " << name;

        const std::optional<CommandInfo> info = findCommandInfo(name);
        if (!info) {
            ToolResult unknown;
            unknown.isError = true;
            unknown.text = "unknown tool: " + name;
            onResult(unknown);
            return;
        }

        //! NOTE Same rule as the UI: a command that is disabled right now (no project,
        //! wrong selection, ...) is refused, not run
        if (!commandsState()->commandState(info->command).enabled) {
            ToolResult unavailable;
            unavailable.isError = true;
            unavailable.text = "not available now: " + name;
            onResult(unavailable);
            return;
        }

        Params params;
        Ret ret = toParams(*info, args, params);
        if (!ret) {
            onResult(toToolResult(Response { 0, ret, {} }));
            return;
        }

        //! NOTE Answer when the command has finished, with its result
        commandsDispatcher()->dispatch(info->command, params).onResolve(this, [onResult](const Response& response) {
            onResult(toToolResult(response));
        });
    });

    if (!m_mcpServer->init()) {
        m_mcpServer = nullptr;
        return false;
    }
    return true;
}

void McpController::stop()
{
    if (m_mcpServer) {
        m_mcpServer->deinit();
        m_mcpServer = nullptr;
    }
}

std::optional<CommandInfo> McpController::findCommandInfo(const std::string& toolName) const
{
    //! NOTE Look the command up by its tool name: turning '_' back into '/' would
    //! break every command whose path contains an underscore.
    for (const CommandInfo& info : commandsRegister()->commandInfoList()) {
        if (info.availabilities.testFlag(Availability::MCP) && commandToToolName(info.command) == toolName) {
            return info;
        }
    }
    return std::nullopt;
}

std::vector<Tool> McpController::makeToolsList() const
{
    std::vector<Tool> tools;
    auto commandList = commandsRegister()->commandInfoList();
    tools.reserve(commandList.size());
    for (const auto& info : commandList) {
        if (!info.availabilities.testFlag(Availability::MCP)) {
            continue;
        }

        Tool tool;
        tool.name = commandToToolName(info.command);
        tool.title = info.title.raw().translated().toStdString();
        tool.description = info.description.translated().toStdString();
        for (const auto& [name, arg] : info.inputSchema.args) {
            Property property;
            property.type = toMcpType(arg.type);
            property.description = arg.description.toStdString();
            if (!arg.min.isNull()) {
                property.minimum = toJson(arg.min);
            }
            if (!arg.max.isNull()) {
                property.maximum = toJson(arg.max);
            }
            tool.inputSchema.properties[name] = property;
        }
        tools.push_back(std::move(tool));
    }
    return tools;
}
