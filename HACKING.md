# Coding style

We use mainly the "llvm" coding style, with some minor deviations.
See .clang-format and .clang-tidy for the details.

# External libraries
Libraries that are not so common in the Ubuntu repositories are imported as source code  
by means of
[Git Submodules](https://git-scm.com/book/en/v2/Git-Tools-Submodules).
This is not without problems. 
See:

[Why do am i getting error 404 on my sub module symlink found on my git repo?](https://www.nuomiphp.com/eplan/en/199186.html)

finally I did (through the CLion interface):
13:17:56.378: [lib/spdlog] git -c core.quotepath=false -c log.showSignature=false checkout v1.8.0^0 --
HEAD is now at 4a9ccf7... Fixed chrono wrapper

and 

13:34:28.472: [tests/lib/googletest] git -c core.quotepath=false -c log.showSignature=false checkout release-1.10.0^0 --
HEAD is now at 703bd9c... Googletest export
