toxvpn
======

[![Build Status](https://travis-ci.org/cleverca22/toxvpn.svg?branch=master)](https://travis-ci.org/cleverca22/toxvpn)

**toxvpn** is a powerful tool that allows one to make tunneled point to point connections over [Tox](https://github.com/irungentoo/toxcore).

Using Tox for transport allows fast, efficient and reliable encrypted tunneling.

Currently only Linux has full support.


## Documentation:
* [Installation](INSTALL.md)


To run **toxvpn** after you compile / install it, you will need to load ``tun`` module:
```
# modprobe tun
```

After that, you can run **toxvpn**:
```
# ./toxvpn -i 192.168.127.1
```

After that type ``help`` to get list of commands.


Note that **toxvpn** instances that connect to each other need to have different IPs in order to work properly.


## License
**toxvpn** is licensed under GPLv3. For details, look in [COPYING](COPYING).
