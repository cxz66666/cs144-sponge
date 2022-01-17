# mysponge

an demo implementation for TCP, repo for learning mit cs144

## Sponge quickstart

To set up your build directory:

    $ mkdir -p <path/to/sponge>/build
    $ cd <path/to/sponge>/build
    $ cmake ..

**Note:** all further commands listed below should be run from the `build` dir.

To build:

    $ make

To test (after building; make sure you've got the [build prereqs](https://web.stanford.edu/class/cs144/vm_howto) installed!)

    $ make check_labN *(replacing N with a checkpoint number)*
