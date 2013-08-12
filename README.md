## P1stream

This is early work on a utility to livestream your desktop and camera sources
on a Mac. It will target OS X 10.8 (Mountain Lion) and later.

There's [a Trello board][todo] with a rough to-do list.

 [todo]: https://trello.com/b/mPsCUmiF/p1stream

### Build

The only requirements are Xcode 4 and OS X 10.8. If the project fails to
build, please file an issue.

Check out the code as follows:

    git clone https://github.com/stephank/P1stream.git
    cd P1stream
    git submodule update --init

When updating an existing clone, don't forget about submodules:

    git pull
    git submodule update

Open the workspace, and build the ‘P1stream’ scheme. Or from the command line:

    xcodebuild -workspace P1stream.xcworkspace -scheme P1stream

The binary can be found in the ‘Products’ group of the project navigator. The
filesystem location for this is somewhere in `~/Library/Developer/Xcode/DerivedData`.

### Running

Create a configuration using the sample plist file. The ‘P1stream’ scheme
looks for a `config.plist` in the toplevel directory, so:

    cp ./P1stream/sample_config.plist ./config.plist

Now simply run from the ‘P1stream’ scheme, or from the command-line:

    path/to/P1stream ./config.plist

You can get an excellent local test setup going with [nginx-rtmp-module].

 [nginx-rtmp-module]: https://github.com/arut/nginx-rtmp-module

### License

All original code here is licensed [GPLv3](LICENSE). This is pretty much
everything in this repository (not submodules) except the contents of the
`generated` directories, which is based on their respective projects.

© 2013 — Stéphan Kochen
