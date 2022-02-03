liquidctl daemon
================

https://github.com/liquidctl/liquidctl/discussions/357

At some point, this will be a cross-platform daemon/service with D-Bus
interface:

- A userspace driver for various RGB/fan controllers and liquid coolers

- And also a modern replacement for fancontrol/lm-sensors

Current state
-------------

Can't even call it "a prototype" yet. Just a starting point.

Initializes and monitors one NZXT RGB&Fan controller on Linux through hidraw,
no D-Bus at all.
