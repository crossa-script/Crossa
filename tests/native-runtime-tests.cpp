#include <atomic>
#include <future>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "crossa/compiler/ir/IrDeclaration.h"
#include "crossa/compiler/ir/Program.h"
#include "crossa/compiler/source/SourceLocation.h"
#include "crossa/compiler/types/SemanticType.h"
#include "crossa/network/response/ResponseDecoder.h"
#include "crossa/runtime/errors/CrossaException.h"
#include "crossa/runtime/objects/NativeList.h"
#include "crossa/runtime/objects/NativeModel.h"
#include "crossa/runtime/result/CrossaState.h"
#include "crossa/runtime/scheduler/TaskScheduler.h"
#include "crossa/utils/Log.h"

using namespace std;

namespace crossa::tests {

// Verifies native result states, structured errors, typed decoding, and cancellation.
// run() executes deterministic tests without external services or timing sleeps.
class NativeRuntimeTests final {
public:
    // Executes every native runtime contract test.
    static void run() {
        verifyStateAndErrorContract();
        verifyTypedResponseDecoding();
        verifyDecoderErrors();
        verifyQueuedCancellation();
        verifyExecutingCancellation();
        verifyShutdownCancellation();
    }

private:
    // Verifies exclusive states and structured error metadata.
    static void verifyStateAndErrorContract() {
        runtime::CrossaState<int> success =
            runtime::CrossaState<int>::success(7);
        require(success.isSuccess() && success.getData() == 7,
                "Success state did not preserve typed data.");

        runtime::CrossaError httpError =
            runtime::CrossaError::httpStatus(503);
        runtime::CrossaState<int> failed =
            runtime::CrossaState<int>::failed(httpError);
        require(failed.isFailed(), "Failed state was not terminal failure.");
        require(failed.getError().getHttpStatus() == 503,
                "HTTP status metadata was not preserved.");
        require(failed.getError().isRetryable(),
                "HTTP 503 should be retryable.");
        require(
            runtime::CrossaError::timeout("timeout").getCode() ==
                runtime::CrossaErrorCode::Timeout,
            "Timeout did not preserve its distinct error code."
        );
        require(
            runtime::CrossaError::tls("tls", 35).getNativeCode() == 35,
            "TLS error did not preserve native metadata."
        );

        runtime::CrossaState<int> cancelled =
            runtime::CrossaState<int>::cancelled();
        require(cancelled.isCancelled(),
                "Cancelled state was not terminal cancellation.");
        runtime::CrossaState<int> normalizedCancellation =
            runtime::CrossaState<int>::failed(
                runtime::CrossaError::cancellation()
            );
        require(normalizedCancellation.isCancelled(),
                "Cancellation error was incorrectly exposed as Failed.");

        runtime::RequestHandle requestHandle;
        require(requestHandle.cancel(),
                "First cancellation request should win.");
        require(!requestHandle.cancel(),
                "Cancellation must be idempotent.");
    }

    // Verifies model and list responses become typed native values.
    static void verifyTypedResponseDecoding() {
        compiler::ir::Program program = createPostProgram();
        network::response::ResponseDecoder decoder(program, 4096, 32);
        runtime::RequestHandle requestHandle;
        runtime::RuntimeValue value = decoder.decode(
            "[{\"userId\":1,\"id\":2,\"title\":\"hello\",\"body\":\"world\"}]",
            compiler::types::SemanticType::createList(
                compiler::types::SemanticType::createModel("Post")
            ),
            requestHandle
        );

        require(value.getKind() == runtime::RuntimeValueKind::List,
                "Response was retained as generic JSON instead of NativeList.");
        require(value.getList().getSize() == 1,
                "NativeList size does not match the response.");
        const runtime::RuntimeValue& post = value.getList().get(0);
        require(post.getKind() == runtime::RuntimeValueKind::Model,
                "List element was not decoded into NativeModel.");
        require(post.getModel().getModelName() == "Post",
                "NativeModel did not preserve its semantic type.");
        const runtime::RuntimeValue* title = post.getModel().getField("title");
        require(title != nullptr && title->getString() == "hello",
                "NativeModel field was not decoded into a typed value.");
        require(
            value.format() ==
                "[{\"userId\":1,\"id\":2,\"title\":\"hello\",\"body\":\"world\"}]",
            "Typed native result formatting is not deterministic."
        );
    }

    // Verifies malformed JSON and schema mismatches remain distinguishable.
    static void verifyDecoderErrors() {
        compiler::ir::Program program = createPostProgram();
        network::response::ResponseDecoder decoder(program, 4096, 32);
        runtime::RequestHandle requestHandle;

        requireDecoderError(
            decoder,
            "[{",
            runtime::CrossaErrorCode::InvalidJson,
            requestHandle
        );
        requireDecoderError(
            decoder,
            "[{\"userId\":1}]",
            runtime::CrossaErrorCode::ResponseTypeMismatch,
            requestHandle
        );
        runtime::RequestHandle cancelledHandle;
        (void)cancelledHandle.cancel();
        requireDecoderError(
            decoder,
            "[]",
            runtime::CrossaErrorCode::Cancellation,
            cancelledHandle
        );
    }

    // Verifies cancellation before worker execution skips the task body.
    static void verifyQueuedCancellation() {
        utils::Log log(utils::Log::Level::Error);
        runtime::scheduler::TaskScheduler scheduler(
            runtime::scheduler::SchedulerOptions(1, 4),
            log
        );
        promise<void> blockerStarted;
        future<void> blockerEntered = blockerStarted.get_future();
        promise<void> releaseBlocker;
        shared_future<void> releaseSignal =
            releaseBlocker.get_future().share();

        runtime::scheduler::ScheduledTask blocker = scheduler.submit(
            [&blockerStarted, releaseSignal](const runtime::RequestHandle&) {
                blockerStarted.set_value();
                releaseSignal.wait();
                return runtime::RuntimeValue::createUnit();
            }
        );
        blockerEntered.wait();

        atomic<bool> executed{false};
        runtime::scheduler::ScheduledTask queued = scheduler.submit(
            [&executed](const runtime::RequestHandle&) {
                executed.store(true, memory_order_release);
                return runtime::RuntimeValue::createUnit();
            }
        );
        require(queued.cancel(), "Queued task cancellation was not accepted.");
        releaseBlocker.set_value();
        require(blocker.await().isSuccess(), "Blocking task did not complete.");
        require(!blocker.cancel(),
                "Cancellation after terminal completion must be rejected.");
        require(queued.await().isCancelled(),
                "Queued task did not finish as Cancelled.");
        require(!executed.load(memory_order_acquire),
                "Cancelled queued task executed its body.");
    }

    // Verifies cancellation while executing reaches the task safely.
    static void verifyExecutingCancellation() {
        utils::Log log(utils::Log::Level::Error);
        runtime::scheduler::TaskScheduler scheduler(
            runtime::scheduler::SchedulerOptions(1, 4),
            log
        );
        promise<void> taskStarted;
        future<void> taskEntered = taskStarted.get_future();
        runtime::scheduler::ScheduledTask task = scheduler.submit(
            [&taskStarted](const runtime::RequestHandle& requestHandle) {
                taskStarted.set_value();
                while (!requestHandle.isCancellationRequested()) {
                    this_thread::yield();
                }
                requestHandle.throwIfCancellationRequested();
                return runtime::RuntimeValue::createUnit();
            }
        );
        taskEntered.wait();
        require(task.cancel(), "Executing task cancellation was not accepted.");
        require(task.await().isCancelled(),
                "Executing task did not finish as Cancelled.");
    }

    // Verifies scheduler shutdown cancels active work before joining workers.
    static void verifyShutdownCancellation() {
        utils::Log log(utils::Log::Level::Error);
        runtime::scheduler::TaskScheduler scheduler(
            runtime::scheduler::SchedulerOptions(1, 4),
            log
        );
        promise<void> taskStarted;
        future<void> taskEntered = taskStarted.get_future();
        runtime::scheduler::ScheduledTask task = scheduler.submit(
            [&taskStarted](const runtime::RequestHandle& requestHandle) {
                taskStarted.set_value();
                while (!requestHandle.isCancellationRequested()) {
                    this_thread::yield();
                }
                return runtime::RuntimeValue::createUnit();
            }
        );
        taskEntered.wait();
        scheduler.shutdown();
        require(task.await().isCancelled(),
                "Shutdown did not cancel active scheduler work.");
    }

    // Creates the immutable IR schema used by response-decoder tests.
    static compiler::ir::Program createPostProgram() {
        compiler::source::SourceLocation location(1, 1);
        vector<compiler::ir::IrModelField> fields;
        fields.emplace_back(
            "userId",
            compiler::types::SemanticType::createInt(),
            location
        );
        fields.emplace_back(
            "id",
            compiler::types::SemanticType::createInt(),
            location
        );
        fields.emplace_back(
            "title",
            compiler::types::SemanticType::createString(),
            location
        );
        fields.emplace_back(
            "body",
            compiler::types::SemanticType::createString(),
            location
        );
        vector<unique_ptr<compiler::ir::IrDeclaration>> declarations;
        declarations.push_back(make_unique<compiler::ir::IrModelDeclaration>(
            "Post",
            std::move(fields),
            location
        ));
        return compiler::ir::Program(
            "native-runtime-tests.cra",
            "native-runtime-tests",
            std::move(declarations)
        );
    }

    // Verifies one decoder call produces the expected structured error code.
    static void requireDecoderError(
        const network::response::ResponseDecoder& decoder,
        const string& body,
        runtime::CrossaErrorCode expectedCode,
        const runtime::RequestHandle& requestHandle
    ) {
        try {
            (void)decoder.decode(
                body,
                compiler::types::SemanticType::createList(
                    compiler::types::SemanticType::createModel("Post")
                ),
                requestHandle
            );
        } catch (const runtime::CrossaException& error) {
            require(error.getError().getCode() == expectedCode,
                    "Decoder produced the wrong CrossaError code.");
            return;
        }
        throw runtime_error("Decoder did not produce the expected error.");
    }

    // Throws when one deterministic test condition is false.
    static void require(bool condition, const string& message) {
        if (!condition) {
            throw runtime_error(message);
        }
    }
};

}

// Runs the native runtime test suite and reports failure through the exit code.
int main() {
    crossa::tests::NativeRuntimeTests::run();
    return 0;
}
