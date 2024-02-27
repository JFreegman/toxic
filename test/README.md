# Integration test suite

`test/run` builds a docker image with tui-test and toxic in it and then runs
integration tests, i.e. launch the toxic binary in a virtual terminal and
mechanically interacts with it via stdin/stdout I/O. At the end of a test, it
may take a "screenshot" (black and white copy of the current screen) and
compare it against goldens, similar to Jest Snapshot Testing.

To update the goldens (when changing something in the UI), run `test/run -u`.
