# Install instructions

## Dependencies

| Name      | Version  |
|-----------|----------|
| CMake     | >= 2.6   |
| JsonCpp   | >= 0.5.0 |
| GCC       | >= 4.7   |
| toxcore   | latest   |

## Linux

### Simple install

#### Gentoo
If you are using Gentoo, there is ebuild available in [Tox Gentoo overlay](https://github.com/Tox/gentoo-overlay-tox).

If you don't run Gentoo, you can always compile manually.

### Compiling manually

Make sure to have dependencies installed.

After you install dependencies, run ``cmake`` to generate config:
```
$ cmake .
```

Compile:
```
$ make
```

Now you have **toxvpn** compiled.