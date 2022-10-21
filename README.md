# Circa - carbon nice scripting

> _ca._, abbreviation for _circa (latin)_, meaning approximately


## Installation

Download a [release](https://github.com/woodcoder/circa/releases) and build it
using something like the following:

```
curl -LO https://github.com/woodcoder/circa/releases/download/v0.1/circa-0.1.tar.gz
./configure
make
sudo make install
```

(You will need to ensure that you have the Curl and Jansson libraries
installed.  See the **Building on...** sections below for easy package manager
one liners to do this.)


## Usage

```
ca [option ...] hours [command [argument ...]]
```

The `ca` command will check the Carbon Aware API for the optimal time in the
next few *HOURS* to run the *COMMAND* to make use of the lowest forecast carbon
intensity energy.  If no *COMMAND* is supplied, the program will just block
(sleep) until that time.

Circa is a project for [Carbon Hack 22](https://taikai.network/en/gsf/hackathons/carbonhack22/overview).

### Options

<dl>
  <dt><b>-l</b> &lt;location&gt;</dt>
  <dd>
    specify
    <a href="https://github.com/Green-Software-Foundation/carbon-aware-sdk/blob/dev/src/CarbonAware.LocationSources/CarbonAware.LocationSources.Azure/src/azure-regions.json">
      location
    </a>
    to check for carbon intensity
  </dd>
  <dt><b>-d</b> &lt;duration&gt;</dt>
  <dd>estimated window of runtime of command/task in minutes</dd>
  <dt><b>-u</b> &lt;api url&gt;</dt>
  <dd>url prefix of Carbon Aware API server to consult</dd>
</dl>

### Configuration

Defaults for the _url_ and _location_ settings can be configured system-wide or
per user by using a [config file](circa.conf).  This should be placed in
`/etc/circa.conf` or/and `~/.circa/config`.  These defaults are overridable by
the command line options above.

### Example

On macOS, you could run the following to pop up a reminder to charge your latop for a couple of hours:

```
./ca -d 120 8 osascript -e 'display notification "Why not plug in your charger?" with title "Low Carbon Energy Available"'
```


## Motivation

Inspiration for circa came from thinking about the question "what's the
simplest thing that just might work?!".

It would be great to replace cron with a carbon aware version, as there are
very many daily jobs that are not time sensitive, and that are just kicked off
at midnight, 4am, 6am or similar times because the ops team have to pick a
'quiet' time for them.

However, replacing cron on a system feels like a tricky business.  It's a core
bit of server infrastructure, so there are trust and security issues.

You'd also need to change the crontab syntax to add in options for a deadline
by when you'd like the task to start by and, ideally information about how long
it might take to run.  To make this backwards compatible would be tricky too.

Hmmm.

So, what about trying to create something that's sort of unix-y, kind of in the 
composable component philosophy zone?  Something a bit `nice` maybe?  Or
perhaps in keeping with `at` and `batch`?  What might that look like?

```
ca 6h my-cpu-intensive-script.sh
```

Oh, that looks pretty easy to use!

You could also use it in a shell script around specific commands.

And, maybe, you could use it in a crontab after all:

```
0 0 * * * /usr/local/bin/ca 6h /path/to/my/daily-report-script.sh
```

But, hang on a mo - if that took off, then actually, it might provide quite a
neat configuration syntax for an carbon aware cron (cacron) too?  Like a sort
of incremental adoption path where sysadmins could just install `ca` to time
shift one-off tasks.  And then, in time, could update cron, which would skip
calling `ca` and just use it as a syntax to inform its own overarching carbon
aware schedule.

And that would leave me not having to worry when I schedule my reporting or
update checking scripts - I could just let cacron crack on with it whenever the
local grid's carbon intensity was at its lowest.

Happy days.


## Contributing

Feel free to contact me (woodcoder) on github, taikai or discord.

### Building on macOS

```
brew install pkg-config autoconf automake libtool libcurl jansson
autoreconf -fi -I /usr/local/opt/curl/share/aclocal
./configure
make
```

### Building on Ubuntu

```
sudo apt-get install -y libjansson-dev libcurl4-openssl-dev
autoreconf -fi
./configure
make
```
