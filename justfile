bootstrap:
    cmake -B build
    ln -sf build/compile_commands.json compile_commands.json

format:
    rg --files src -g '*.{hh,cc}' -0 | xargs -0 clang-format -i
    rg --files apps -g '*.{hh,cc}' -0 | xargs -0 clang-format -i
    rg --files util -g '*.{hh,cc}' -0 | xargs -0 clang-format -i
