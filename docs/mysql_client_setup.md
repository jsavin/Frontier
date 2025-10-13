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

## Windows builds (legacy)

Legacy Visual Studio project files have been removed. When Windows support is
reintroduced, updated build instructions will land alongside the new project
files.

## Verifying the setup

After installing the client libraries, run the CLI build as a smoke test:

```bash
make -C frontier-cli
```

Windows build guidance will accompany the future replacement project files.
