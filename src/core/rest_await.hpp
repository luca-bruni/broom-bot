#pragma once

#include "core/jobs.hpp"

#include <chrono>
#include <future>
#include <memory>
#include <optional>
#include <utility>

namespace broom {

// Runs an async DPP REST call from a job's worker thread and blocks until it
// completes or the job is cancelled.
//
// `start` receives a completion callback `done(T)`; call it exactly once from
// the DPP callback with the result. The promise is shared_ptr-owned so
// abandoning the wait on cancellation can't dangle the callback (see
// ARCHITECTURE.md, "REST from the worker"). Returns nullopt on cancellation.
//
//   auto ok = await_rest<bool>(ctx, [&](auto done) {
//       ctx.bot.message_delete(id, channel,
//           [done](const dpp::confirmation_callback_t& cb) { done(!cb.is_error()); });
//   });
template <typename T, typename Start>
std::optional<T> await_rest(const JobContext& ctx, Start&& start) {
    auto promise = std::make_shared<std::promise<T>>();
    auto future = promise->get_future();
    std::forward<Start>(start)(
        [promise](T result) { promise->set_value(std::move(result)); });
    while (future.wait_for(std::chrono::milliseconds(200)) !=
           std::future_status::ready) {
        if (ctx.cancelled()) return std::nullopt;
    }
    return future.get();
}

} // namespace broom
