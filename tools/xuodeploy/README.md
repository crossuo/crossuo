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
$ cp crossuo-build/src/crossuo.so release/linux/crossuo/
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
