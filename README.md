This is an environment for C application. As such the main component is 
Tools.a, in the lib directory.

It includes a tree based memory management, fibers and threads tied to that 
tree structure and responsible for their local subtree.

The project is organised in a directory tree. Each level has its own /lib
and /inc directory. The shared interfaces and libraries are located toward
the root, while more application specific interfaces are located toward 
the leaf.

In addition to their own worse, the leaf applications are here to stress
test the root shared libraries/interface.
Application components that prove to be very useful might migrate toward
the root.

