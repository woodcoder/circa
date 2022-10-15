# Circa - carbon nice scripting

> _ca._, abbreviation for _circa (latin)_, meaning approximately

## Usage

```
ca <HOURS> <COMMAND> [<ARGS>]
```

The `ca` command will check the Carbon Aware API for the optimal time in the
next few *HOURS* to run the *COMMAND*.  It currently assumes a *eastus* server
location and a running window of *30* minutes.

Circa is a project for [Carbon Hack 22](https://taikai.network/en/gsf/hackathons/carbonhack22/overview).

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
