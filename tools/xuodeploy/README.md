<!--
SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein
SPDX-License-Identifier: AGPL-3.0-or-later
-->
X:UO Deploy
===
**xuodeploy** is an internal tool for deploying CrossUO updates

## Using

Initialize the depot
```bash
$ ./xuodeploy --init
```

Initialize a new product for some platform, ie. CrossUO on Linux

```bash
$ ./xuodeploy --platform linux --product "CrossUO" --init
release/linux/crossuo initialized.
```

Copy the release files in the generated directory:
```bash
$ cp crossuo-build/tools/xoui/xuolauncher release/linux/crossuo/
$ cp crossuo-build/src/crossuo release/linux/crossuo/
```

(The in-client assistant build produces a single self-contained `crossuo`
executable; with `XUO_ASSISTANT=OFF` also copy
`crossuo-build/src/crossuo.so`, the plugin-layout shared library.)
```

Generate the specific product update data by specifying the desired version number:
```bash
$ ./xuodeploy --platform linux --product "CrossUO" --version "1.1.0" --force
```

And finally, to start a development test server:
```bash
$ ./xuodeploy --serve
listening to http://localhost:12593
```
