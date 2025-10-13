# MySQL / MariaDB Client Setup

Frontier still links against the classic MySQL client API for historical
reasons. The repository no longer vendors prebuilt client libraries, so you must
install them locally before building Frontier.

## macOS / Linux

Run the helper script and it will download, build, and install a universal
(static) MariaDB Connector/C into the ignored `Common/MySQL/` directory:

```bash
scripts/build_mysql_client.sh
```

By default the script installs to `Common/MySQL/`. Override this by exporting
`MYSQL_CLIENT_PREFIX` before running it:

```bash
MYSQL_CLIENT_PREFIX=$HOME/mariadb-client scripts/build_mysql_client.sh
```

The script requires `cmake` and `curl`. It populates:

- `lib/libmysqlclient.a` (symlink to `libmariadb.a`)
- `include/mysql/` (symlink to the connector headers)

## Windows (Visual Studio builds)

1. Download the MariaDB Connector/C package that matches your toolchain from
   <https://mariadb.com/downloads/connectors/c/>. Install it somewhere outside
   the repository (for example `C:\dev\mariadb-connector-c`).
2. Set the environment variable `MYSQL_CLIENT_DIR` to that installation root
   before opening the Frontier solutions. The Visual C++ projects expect to find
   headers under `include\mysql` and libraries under `lib`.
3. Alternatively, copy the `include` and `lib` folders from the MariaDB
   installation into `Common\MySQL\`. The `.gitignore` entry keeps these
   artefacts out of version control.

Both Visual Studio projects now link against either `libmariadb.lib` (from the
MariaDB connector) or `mysqlclient.lib` (for compatibility with Oracle's MySQL
client). Ensure that the library you have installed exposes at least one of
those names.

## Verifying the setup

After installing the client libraries, run the CLI build as a smoke test:

```bash
make -C frontier-cli
```

For Visual Studio builds, rebuild the Frontier solution. If the MySQL client is
missing you will see linker errors for `mysql_*` symbols—double-check the
`MYSQL_CLIENT_DIR` variable and the connector installation.
