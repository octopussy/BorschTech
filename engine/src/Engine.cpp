#include "daScript/daScript.h"

#include "Engine.h"
#include "core/Logging.h"
#include "input/InputManager.h"

using namespace das;

// function, which we are going to expose to daScript
float xmadd(float a, float b, float c, float d) {
    return a * b + c * d;
}

// making custom builtin module
class EngineModule : public das::Module {
public:
    EngineModule() : Module("engine") {   // module name, when used from das file
        das::ModuleLibrary lib;
        lib.addModule(this);
        lib.addBuiltInModule();
        // adding constant to the module
        addConstant(*this, "SQRT2", sqrtf(2.0));
        // adding function to the module
        das::addExtern<DAS_BIND_FUN(xmadd)>(*this, lib, "xmadd", das::SideEffects::none, "xmadd");
    }
};

REGISTER_MODULE(EngineModule);

void run_das(const std::string &ProjectRootScript, const char *MainFnName) {
    das::TextPrinter tout;                               // output stream for all compiler messages (stdout. for stringstream use TextWriter)
    das::ModuleGroup dummyLibGroup;                      // module group for compiled program
    auto fAccess = das::make_smart<das::FsFileAccess>();      // default file access
    // compile program

    auto program = compileDaScript(ProjectRootScript, fAccess, tout, dummyLibGroup);
    if (program->failed()) {
        // if compilation failed, report errors
        tout << "failed to compile\n";
        for (auto &err: program->errors) {
            tout << reportError(err.at, err.what, err.extra, err.fixme, err.cerr);
        }
        return;
    }
    // create daScript context
    das::Context ctx(program->getContextStackSize());
    if (!program->simulate(ctx, tout)) {
        // if interpretation failed, report errors
        tout << "failed to simulate\n";
        for (auto &err: program->errors) {
            tout << reportError(err.at, err.what, err.extra, err.fixme, err.cerr);
        }
        return;
    }
    // find function 'test' in the context
    auto fnTest = ctx.findFunction(MainFnName);
    if (!fnTest) {
        tout << "function '" << MainFnName << " not found\n";
        return;
    }
    // verify if 'test' is a function, with the correct signature
    // note, this operation is slow, so don't do it every time for every call
    if (!das::verifyCall<void>(fnTest->debugInfo, dummyLibGroup)) {
        tout << "function " << MainFnName << ", call arguments do not match. expecting def test : void\n";
        return;
    }
    // evaluate 'test' function in the context
    ctx.eval(fnTest, nullptr);
    if (auto ex = ctx.getException()) {       // if function cased panic, report it
        tout << "exception: " << ex << "\n";
        return;
    }
}

namespace bt {

    std::unique_ptr<Engine> GEngine;
    std::unique_ptr<input::InputManager> GInputManager;


    void Engine::Init(const string &projectRoot, const string &dasRoot) {
        log::GLogger = std::make_unique<log::Logger>();
        GInputManager = std::make_unique<input::InputManager>();
        GInputManager->Init();

        das::setDasRoot(dasRoot);

        printf("ENGINE START!!!\n");

        NEED_ALL_DEFAULT_MODULES;
        NEED_MODULE(EngineModule);
        das::Module::Initialize();

        run_das(projectRoot + "/main.das", "main");
    }

    void Engine::Shutdown() {
        das::Module::Shutdown();
    }
}
