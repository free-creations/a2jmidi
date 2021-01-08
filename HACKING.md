# Coding style

We use mainly the "llvm" coding style, with some minor deviations.
See .clang-format and .clang-tidy for the details.

# External libraries
Libraries that are not so common in the Ubuntu repositories are imported as source code  
by means of
[Git Submodules](https://git-scm.com/book/en/v2/Git-Tools-Submodules).
This is not without problems. 

See also:
[Why do am i getting error 404 on my sub module symlink found on my git repo?](https://www.nuomiphp.com/eplan/en/199186.html)

# setting the _spdlog_ submodule to version 1.8.1

in `[project root]/lib/spdlog` do:
```commandline
$ git fetch origin
$ git reset --hard v1.8.1
```

see also 
[Git submodules best practices](https://gist.github.com/slavafomin/08670ec0c0e75b500edbaa5d43a5c93c)