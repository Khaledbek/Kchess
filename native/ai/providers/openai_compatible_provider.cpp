#include "provider_adapters.h"

#include <string>
#include <utility>

#include <nlohmann/json.hpp>

#include "coach_response_json.h"
#include "provider_prompt.h"
#include "provider_transport.h"

namespace kchess::ai {
namespace {

using json = nlohmann::json;

// -----------------------------------------------------------------------------
// Section: Request serialization
// -----------------------------------------------------------------------------

// JSON mode guarantees a JSON object but not its shape, so the schema travels
// in the instruction. Native validation still decides what is accepted.
std::string json_output_instruction() {
  return " Return exactly one JSON object and no other text. It must conform "
         "to this JSON Schema: " +
         coach_response_json::schema().dump();
}

json request_body(const LLMProviderRequest& request, const ProviderConfig& config) {
  json system_message = {
      {"role", "system"},
      {"content", coach_system_instruction(request) + json_output_instruction()}};
  json user_message = {{"role", "user"},
                       {"content", coach_provider_input(request)}};
  json messages = json::array();
  messages.push_back(std::move(system_message));
  messages.push_back(std::move(user_message));

  json body = {
      {"model", config.model},
      {"messages", std::move(messages)},
      {"response_format", {{"type", "json_object"}}},
      {"stream", false},
  };
  body[config.max_tokens_field] = config.max_output_tokens;
  if (!config.effort.empty()) body["reasoning_effort"] = config.effort;
  if (!config.thinking.empty()) {
    body["thinking"] = {{"type", config.thinking}};
  }
  return body;
}

// -----------------------------------------------------------------------------
// Section: Response parsing
// -----------------------------------------------------------------------------

// Truncated output (finish_reason "length") fails JSON parsing and is reported
// as an invalid response.
StructuredCoachContent parse_response(const std::string& body) {
  const auto root = json::parse(body);
  if (!root.contains("choices") || !root["choices"].is_array() ||
      root["choices"].empty()) {
    return {};
  }
  const auto& message = root["choices"][0].at("message");
  if (!message.contains("content") || !message["content"].is_string()) return {};
  const auto text = message["content"].get<std::string>();
  if (text.empty()) return {};
  const auto structured = json::parse(text);
  if (!structured.contains("answer_segments") ||
      !structured["answer_segments"].is_array()) {
    return {};
  }
  return coach_response_json::parse(structured);
}

}  // namespace

// -----------------------------------------------------------------------------
// Section: Chat-completions transport
// -----------------------------------------------------------------------------

LLMProviderResult complete_openai_compatible(const ProviderConfig& config,
                                             const LLMProviderRequest& request,
                                             const RemoteProviderConfig& remote) {
  LLMProviderResult failure;
  const auto body = post_provider_request(
      config, request, remote, request_body(request, config).dump(),
      {{"Authorization", "Bearer " + config.api_key}}, failure);
  if (!body) return failure;
  return provider_content_result(config, remote, *body, parse_response);
}

}  // namespace kchess::ai
