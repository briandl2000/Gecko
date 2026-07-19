# Development workflow

Use three increasingly strict loops:

1. `python3 build.py engine` while changing engine internals.
2. `python3 build.py sandbox` for dirty experiments using the launcher and plugin in `projects/`.
3. `python3 build.py sdk-test` before pushing to prove that public headers and packaged libraries are sufficient.

The sandbox is intentionally disposable. Its baseline opens a platform window, creates a Vulkan swapchain, clears and presents it, and draws animated lines through the Debug Renderer module. The headless Null path remains available for SDK smoke checks. It may temporarily contain the rendering, audio, input, or plugin experiment currently being developed, but should remain small enough to rewrite.

Every successful build refreshes the ignored `compile_commands.json` used by Zed/clangd. `python3 build.py clean` removes both compiled output and that generated editor database.

`sdk-test` uses the same sandbox source but a different dependency path. It stages an SDK under `out/sdk-test/sdk`, invokes the copied SDK driver, builds into `out/sdk-test/consumer`, and runs the result. It must not use private Gecko headers or source files.

For longer experiments, create a module in `../Gecko_demos`, change into that demo directory, and build it with:

```sh
python3 /path/to/Gecko/build.py .
```

To reproduce the downloaded-user workflow exactly, first run `python3 build.py sdk`, then invoke `out/sdk/build.py` from the demo repository. A throwaway project may also live under the ignored `scratch/` directory.

Windows should follow the same commands and directory conventions from an MSVC Developer Command Prompt. Shared-drive and remote-machine details will be defined separately after the Windows SDK path has passed CI.
