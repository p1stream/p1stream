## P1stream

This is early work on a utility to livestream your desktop and camera sources
on a Mac. It's compatible with 10.8 (Mountain Lion) and 10.9 (Mavericks).

There's [a Trello board][todo] with a rough to-do list.

 [todo]: https://trello.com/b/mPsCUmiF/p1stream

### Build

The only requirement is Xcode 5. If the project fails to build, file an issue.

Check out the code and submodules, then build and run the ‘P1stream’ scheme.

### CLI

There's also a command-line interface. The steps to get it running are:

    # Check out the code and submodules.
    git clone https://github.com/stephank/P1stream.git
    cd P1stream
    git submodule update --init

    # Build the ‘P1stream-cli’ scheme.
    xcodebuild -workspace P1stream.xcworkspace -scheme P1stream-cli

    # Create a configuration, based on the sample.
    cp ./P1stream/cli/sample_config.plist ./config.plist
    $EDITOR ./config.plist

    # Run!
    path/to/P1stream ./config.plist

### Testing

You can get an excellent local test setup going with [nginx-rtmp-module].

 [nginx-rtmp-module]: https://github.com/arut/nginx-rtmp-module

### License

All original code here is licensed [GPLv3](LICENSE). This is pretty much
everything in this repository (not submodules) except the contents of the
`generated` directories, which is based on their respective projects.

© 2013 — Stéphan Kochen
