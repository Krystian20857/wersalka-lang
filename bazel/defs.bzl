WersalkaInfo = provider(
    fields = {"files": "depset of .w File objects"},
)

def _wersalka_library_impl(ctx):
    transitive = [dep[WersalkaInfo].files for dep in ctx.attr.deps]
    all_files = depset(ctx.files.srcs, transitive = transitive)
    return [WersalkaInfo(files = all_files), DefaultInfo(files = all_files)]

wersalka_library = rule(
    implementation = _wersalka_library_impl,
    attrs = {
        "srcs": attr.label_list(allow_files = [".w"]),
        "deps": attr.label_list(providers = [WersalkaInfo]),
    },
)

_VM_ATTR = {
    "_vm": attr.label(
        default = Label("//runtime:vm"),
        executable = True,
        cfg = "exec",
    ),
}

def _build_vm_script(ctx, argv_files, runfiles_files):
    vm = ctx.executable._vm
    workspace = ctx.workspace_name

    def runfiles_path(file):
        return '"$R/%s"' % file.short_path

    script = ctx.actions.declare_file(ctx.label.name)
    ctx.actions.write(
        output = script,
        content = "\n".join([
            "#!/bin/bash",
            'if [[ -z "${RUNFILES_DIR:-}" ]]; then',
            '  RUNFILES_DIR="${BASH_SOURCE[0]}.runfiles"',
            "fi",
            'R="${RUNFILES_DIR}/%s"' % workspace,
            "exec %s %s" % (
                runfiles_path(vm),
                " ".join([runfiles_path(file) for file in argv_files]),
            ),
        ]) + "\n",
        is_executable = True,
    )

    runfiles = ctx.runfiles(files = [vm] + runfiles_files).merge(
        ctx.attr._vm[DefaultInfo].default_runfiles,
    )
    return [DefaultInfo(executable = script, runfiles = runfiles)]

def _collect_dep_files(ctx):
    lib_files = []
    for dep in ctx.attr.deps:
        lib_files += dep[WersalkaInfo].files.to_list()
    return lib_files

def _wersalka_binary_impl(ctx):
    all_wl = _collect_dep_files(ctx) + [ctx.file.srcs]
    return _build_vm_script(ctx, argv_files = all_wl, runfiles_files = all_wl)

wersalka_binary = rule(
    implementation = _wersalka_binary_impl,
    executable = True,
    attrs = dict({
        "srcs": attr.label(allow_single_file = [".w"], mandatory = True),
        "deps": attr.label_list(providers = [WersalkaInfo]),
    }, **_VM_ATTR),
)

def _wersalka_test_impl(ctx):
    dep_files = _collect_dep_files(ctx)
    argv_files = dep_files + [ctx.file.srcs]
    return _build_vm_script(
        ctx,
        argv_files = argv_files,
        runfiles_files = argv_files + ctx.files.data,
    )

wersalka_test = rule(
    implementation = _wersalka_test_impl,
    test = True,
    attrs = dict({
        "srcs": attr.label(allow_single_file = [".w"], mandatory = True),
        "data": attr.label_list(allow_files = [".w"]),
        "deps": attr.label_list(providers = [WersalkaInfo]),
    }, **_VM_ATTR),
)
