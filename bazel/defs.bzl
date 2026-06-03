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

def _wersalka_binary_impl(ctx):
    vm = ctx.executable._vm
    srcs = ctx.file.srcs
    lib_files = []
    for dep in ctx.attr.deps:
        lib_files += dep[WersalkaInfo].files.to_list()
    all_wl = lib_files + [srcs]

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
                " ".join([runfiles_path(file) for file in all_wl]),
            ),
        ]) + "\n",
        is_executable = True,
    )

    runfiles = ctx.runfiles(files = [vm] + all_wl).merge(
        ctx.attr._vm[DefaultInfo].default_runfiles,
    )
    return [DefaultInfo(executable = script, runfiles = runfiles)]

wersalka_binary = rule(
    implementation = _wersalka_binary_impl,
    executable = True,
    attrs = {
        "srcs": attr.label(allow_single_file = [".w"], mandatory = True),
        "deps": attr.label_list(providers = [WersalkaInfo]),
        "_vm": attr.label(
            default = Label("//runtime:vm"),
            executable = True,
            cfg = "exec",
        ),
    },
)
