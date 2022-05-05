#include <cstdio>
#include "daScript/daScript.h"
#include "entt/entt.hpp"
#include "imgui_vulkan_test.h"

using namespace das;

// function, which we are going to expose to daScript
float xmadd ( float a, float b, float c, float d ) {
    return a*b + c*d;
}

// making custom builtin module
class EngineModule : public Module {
public:
    EngineModule() : Module("engine") {   // module name, when used from das file
        ModuleLibrary lib;
        lib.addModule(this);
        lib.addBuiltInModule();
        // adding constant to the module
        addConstant(*this,"SQRT2",sqrtf(2.0));
        // adding function to the module
        addExtern<DAS_BIND_FUN(xmadd)>(*this, lib, "xmadd", SideEffects::none, "xmadd");
    }
};

// registering module, so that its available via 'NEED_MODULE' macro
REGISTER_MODULE(EngineModule);

#define PROJECT_ROOT_DIR  "D:/_borsh/tech"
#define TEST_DAS_NAME  PROJECT_ROOT_DIR"/engine/test.das"

void run_das() {
    TextPrinter tout;                               // output stream for all compiler messages (stdout. for stringstream use TextWriter)
    ModuleGroup dummyLibGroup;                      // module group for compiled program
    auto fAccess = make_smart<FsFileAccess>();      // default file access
    // compile program

    auto program = compileDaScript(TEST_DAS_NAME, fAccess, tout, dummyLibGroup);
    if ( program->failed() ) {
        // if compilation failed, report errors
        tout << "failed to compile\n";
        for ( auto & err : program->errors ) {
            tout << reportError(err.at, err.what, err.extra, err.fixme, err.cerr );
        }
        return;
    }
    // create daScript context
    Context ctx(program->getContextStackSize());
    if ( !program->simulate(ctx, tout) ) {
        // if interpretation failed, report errors
        tout << "failed to simulate\n";
        for ( auto & err : program->errors ) {
            tout << reportError(err.at, err.what, err.extra, err.fixme, err.cerr );
        }
        return;
    }
    // find function 'test' in the context
    auto fnTest = ctx.findFunction("test");
    if ( !fnTest ) {
        tout << "function 'test' not found\n";
        return;
    }
    // verify if 'test' is a function, with the correct signature
    // note, this operation is slow, so don't do it every time for every call
    if ( !verifyCall<void>(fnTest->debugInfo, dummyLibGroup) ) {
        tout << "function 'test', call arguments do not match. expecting def test : void\n";
        return;
    }
    // evaluate 'test' function in the context
    ctx.eval(fnTest, nullptr);
    if ( auto ex = ctx.getException() ) {       // if function cased panic, report it
        tout << "exception: " << ex << "\n";
        return;
    }
}

struct position {
    float x;
    float y;
};

struct velocity {
    float dx;
    float dy;
};

void update(entt::registry &registry) {
    auto view = registry.view<const position, velocity>();

    // use a callback
    view.each([](const auto &pos, auto &vel) { /* ... */ });

    // use an extended callback
    view.each([](const auto entity, const auto &pos, auto &vel) { /* ... */ });

    // use a range-for
    for (auto [entity, pos, vel]: view.each()) {
        printf("%.3f %.3f", pos.x, pos.y);
    }

    // use forward iterators and get only the components of interest
    for (auto entity: view) {
        auto &vel = view.get<velocity>(entity);
        // ...
    }
}

void run_entt() {
    entt::registry registry;

    for (auto i = 0u; i < 10u; ++i) {
        const auto entity = registry.create();
        registry.emplace<position>(entity, i * 1.f, i * 1.f);
        if (i % 2 == 0) { registry.emplace<velocity>(entity, i * .1f, i * .1f); }
    }

    update(registry);
}

int main(int, char *[]) {

    setDasRoot(std::string(PROJECT_ROOT_DIR"/_thirdparty/daScript"));
    printf("START!!!\n");

    NEED_ALL_DEFAULT_MODULES;
    NEED_MODULE(EngineModule);
    Module::Initialize();

    run_das();
    run_entt();

    imgui_vulkan_test();


    Module::Shutdown();

    return 0;
}


