alcove
------

alcove is an Erlang interface for creating system and application
containers like sandboxes or Linux containers. alcove works by giving
Erlang processes access to the system primitives used for isolating and
controlling Unix processes.

_alcove_ is an external port process (a standalone
Unix process that communicates with the erlang VM using
stdin/stdout). [prx](https://github.com/msantos/prx) is a higher level
library that maps the alcove Unix processes to Erlang processes.

Overview
========

When alcove is started, it enters an event loop:

```erlang
{ok, Drv} = alcove_drv:start().
```

Much like a shell, alcove waits for a command. For example, alcove can
be requested to fork(2):

```erlang
{ok, Child1} = alcove:fork(Drv, []).
```

Now there are 2 processes in a parent/child relationship, sitting in
their event loops:

```
beam.smp
  |-erl_child_setup
  |   `-alcove
  |       `-alcove
```

We access the child process by using the fork chain:

```erlang
{ok, Child2} = alcove:fork(Drv, [Child1]),
Child2 = alcove:getpid(Drv, [Child1,Child2]).
```

An empty fork chain refers to the port process:

```erlang
{ok, Child3} = alcove:fork(Drv, []).
```

Fork chains can be arbitrarily long (well, until you hit a system limit
or overflow the stack) but, by default, are limited to a length of 16
Unix processes.

Finally, we can replace the event loop with a system executable by
calling exec(3):

```erlang
ok = alcove:execvp(Drv, [Child1,Child2], "/bin/cat", ["/bin/cat"]).
```

The process tree now looks like:

```
beam.smp
  |-erl_child_setup
  |   `-alcove
  |       |-alcove
  |       |   `-cat
  |       `-alcove
```

We can interact with the process via stdin, stdout and stderr:

```erlang
alcove:stdin(Drv, [Child1,Child2], "hello process\n"),
<<"hello process\n">> = alcove:stdout(Drv, [Child1,Child2]).
```

Setting Up Privileges
=====================

* sudo

```
sudo visudo -f /etc/sudoers.d/99_alcove
<user> ALL = NOPASSWD: /path/to/alcove/priv/alcove
Defaults!/path/to/alcove/priv/alcove !requiretty
```

When starting alcove, pass in the `exec` option:

```erlang
{ok, Drv} = alcove_drv:start([{exec, "sudo -n"}]).
```

* setuid

```
chown root:root priv/alcove
chmod u+s priv/alcove
```

* Linux: file capabilites

  See capabilities(7) and setcap(8).

Creating a chroot
=================

The standard Unix way of sandboxing a process is by doing a chroot(2). The
chroot process involves:

* running as root
* setting process limits
* changing the root directory to limit the process' view of the filesystem
* changing to an unprivileged user
* running the sandboxed code

See `examples/chrootex.erl`.

We'll create a chroot using an interface like:

```erlang
-spec sandbox(port(), [iodata()]) -> non_neg_integer().
sandbox(Drv, ["/bin/sh", "-i"]).
```

The function returns the system PID of the child process. This would
create an interactive shell we access through standard I/O.

In order to call chroot(2), the port will need root privileges:

```erlang
{ok, Drv} = alcove_drv:start([{exec, "sudo -n"}]).
```

Following the steps outlined earlier, we want to set some process
limits. In this case, we'll use setrlimit(2):

```erlang
setlimits(Drv, Child) ->
    % Disable creation of files
    ok = alcove:setrlimit(Drv, [Child], rlimit_fsize,
            #alcove_rlimit{cur = 0, max = 0}),

    ok = alcove:setrlimit(Drv, [Child], rlimit_nofile,
            #alcove_rlimit{cur = 0, max = 0}),

    % Limit to one process
    ok = alcove:setrlimit(Drv, [Child], rlimit_nproc,
            #alcove_rlimit{cur = 1, max = 1}).
```

Next we chroot and drop root privileges. We will set the user and group
to a random, high UID/GID that is unlikely to conflict with an existing
system user:

```erlang
chroot(Drv, Child, Path) ->
    ok = alcove:chroot(Drv, [Child], Path),
    ok = alcove:chdir(Drv, [Child], "/").

drop_privs(Drv, Child, Id) ->
    ok = alcove:setgid(Drv, [Child], Id),
    ok = alcove:setuid(Drv, [Child], Id).

id() ->
    16#f0000000 + crypto:rand_uniform(0, 16#ffff).
```

Tying it all together:

```erlang
% The default is to run the cat command. Because of the chroot, we need
% to use a statically linked executable.
sandbox(Drv) ->
    sandbox(Drv, ["/bin/busybox", "cat"]).
sandbox(Drv, Argv) ->
    {Path, Arg0, Args} = argv(Argv),

    {ok, Child} = alcove:fork(Drv, []),

    setlimits(Drv, Child),
    chroot(Drv, Child, Path),
    drop_privs(Drv, Child, id()),

    ok = alcove:execvp(Drv, [Child], Arg0, [Arg0, Args]),

    Child.

% Set the program path for the chroot
argv([Arg0, Args]) ->
    Path = filename:dirname(Arg0),
    Progname = filename:join(["/", filename:basename(Arg0)]),
    {Path, Progname, Args}.
```

Compile and run the example:

```
# If alcove is in ~/src/alcove
export ERL_LIBS=~/src
make eg
rebar shell
```

```erlang
1> {ok, Drv} = chrootex:start().

2> Cat = chrootex:sandbox(Drv).
31831

3> alcove:stdin(Drv, [Cat], "test test\n").
4> alcove:stdout(Drv, [Cat]).
<<"test test\n">>
```

We can test the limits of the sandbox by using a shell instead of
herding cat's:

```erlang
5> Sh = chrootex:sandbox(Drv, ["/bin/busybox", "sh"]).
31861

% Test the shell is working
6> alcove:stdin(P, [Sh], "echo hello\n").
ok
7> alcove:stdout(P, [Sh]).
<<"hello\n">>

% Attempt to create a file
6> alcove:stdin(Drv, [Sh], "> foo\n").
ok
7> alcove:stderr(P, [Sh]).
<<"sh: can't create foo: Too many open files\n">>

% Try to fork a new process
8> alcove:stdin(Drv, [Sh], "ls\n").
9> alcove:stderr(P, [Sh]).
<<"sh: can't fork\n">>

% If we check the parent for events, we can see the child has exited
10> alcove:event(P, []).
{signal,sigchld}
```

Creating a Container Using Linux Namespaces
===========================================

Namespaces are the basis for Linux Containers (LXC). Creating a
new namespace is as simple as passing in the appropriate flags to
clone(2). We'll rewrite the chroot example to run inside a namespace and
use another Linux feature, control groups, to limit the system resources
available to the process.

See `examples/nsex.erl`.

* set process limits using cgroups (see cpuset(7))

  When the port is started, we'll create a new cgroup just for our
  application and, whenever a sandboxed process is forked, we'll add it
  to this cgroup.

```erlang
start() ->
    {ok, Drv} = alcove_drv:start([{exec, "sudo -n"}]),

    % Create a new cgroup for our processes
    ok = alcove_cgroup:create(Drv, [], <<"alcove">>),

    % Set the CPUs these processes are allowed to run on. For example,
    % if there are 4 available CPUs, any process in this cgroup will only
    % be able to run on CPU 0
    {ok,1} = alcove_cgroup:set(Drv, [], <<"cpuset">>, <<"alcove">>,
            <<"cpuset.cpus">>, <<"0">>),
    {ok,1} = alcove_cgroup:set(Drv, [], <<"cpuset">>, <<"alcove">>,
            <<"cpuset.mems">>, <<"0">>),

    % Set the amount of memory available to the process

    % Total memory, including swap. We allow this to fail, because some
    % systems may not have a swap partition/file
    alcove_cgroup:set(Drv, [], <<"memory">>, <<"alcove">>,
            <<"memory.memsw.limit_in_bytes">>, <<"16m">>),

    % Total memory
    {ok,3} = alcove_cgroup:set(Drv, [], <<"memory">>, <<"alcove">>,
            <<"memory.limit_in_bytes">>, <<"16m">>),

    Drv.

setlimits(Drv, Child) ->
    % Add our process to the "alcove" cgroup
    {ok,_} = alcove_cgroup:set(Drv, [], <<>>, <<"alcove">>,
            <<"tasks">>, integer_to_list(Child)).

```

* running the code involves calling clone(2) to create the namespaces,
  rather than using fork(2)

```erlang
sandbox(Drv, Argv) ->
    {Path, Arg0, Args} = argv(Argv),

    {ok, Child} = alcove:clone(Drv, [], [
            clone_newipc, % IPC
            clone_newnet, % network
            clone_newns,  % mounts
            clone_newpid, % PID, Child is PID 1 in the namespace
            clone_newuts  % hostname
            ]),

    setlimits(Drv, Child),
    chroot(Drv, Child, Path),
    drop_privs(Drv, Child, id()),

    ok = alcove:execvp(Drv, [Child], Arg0, [Arg0, Args]),

    Child.
```

alcove_drv
==========

    start() -> port()
    start(Options) -> port()
    start_link() -> port()
    start_link(Options) -> port()

    Types   Options = [Option]
            Option = stderr_to_stdout | {env, [{Key, Val}]}
                | {exec, string()}
                | {progname, string()}

    Create the alcove port.

        stderr_to_stdout

            The behaviour of stderr from the port differs from child
            processes. Standard error from the port goes to the console
            while stderr from child processes is tagged and sent to the
            controlling Erlang process.

            This option merges stderr and stdout from the port. Since
            stdout is used for communicating with the Erlang side and
            is tagged with a header, this will likely mess things up.

            Only use this option if you want to call execvp/3,4 in
            the port.

        {env, [{Key,Val}]}

            Set the environment for the port.

        {exec, Exec}

            Default: ""

            Sets a command to run the port, such as sudo.

        {progname, Path}

            Default: priv/alcove

            Sets the path to the alcove executable.

    For the remaining options, see alcove:getopt/2,3.

alcove
======

Functions marked as operating system specific will return
{error,enotsup} on other platforms.

### Data Types

    Drv = pid()
    OSPid = non_neg_integer()
    ForkChain = [OSPid]
    Path = iodata()
    FD = integer()

    constant() = atom() | integer()

    alcove_pid() =
        #alcove_pid{pid = alcove:pid_t(),
                    fdctl = alcove::fd(),
                    stdin = alcove::fd(),
                    stdout = alcove::fd(),
                    stderr = alcove::fd()}

### Event Loop

These functions can be called while the process is running in the event
loop. Using these functions after the process has called exec(3) will
probably confuse the process.

Functions accepting a constant() will return {error, enotsup} if an
atom is used as the argument and is not found on the platform.

    chdir(Drv, ForkChain, Path) -> ok | {error, posix()}

        chdir(2) : change process current working directory.

    children(Drv, ForkChain) -> [Child]

        Types   Child = #alcove_pid{}

        Returns the list of child PIDs for this process.

    chmod(Drv, ForkChain, Path, Mode) -> ok | {error, posix()}

        chmod(2) : change file permissions

    chown(Drv, ForkChain, Path, Owner, Group) -> ok | {error, posix()}

        Types   Owner = Group = non_neg_integer()

        chown(2) : change file ownership

    chroot(Drv, ForkChain, Path) -> ok | {error, posix()}

        chroot(2) : change root directory

    clearenv(Drv, ForkChain) -> ok | {error, posix()}

        clearenv(3) : zero process environment

    clone(Drv, ForkChain, Flags) -> {ok, integer()} | {error, posix()}

        Types   Flags = integer() | [constant()]

        Linux only.

        clone(2) : create a new process

    clone_constant(Drv, ForkChain, atom()) -> integer() | unknown

        Linux only.

        Map symbols to integer constants.

    close(Drv, ForkChain, FD) -> ok | {error, posix()}

        close(2) : close a file descriptor.

    connect(Drv, ForkChain, Fd, Sockaddr) -> ok | {error, posix()}

        connect(2) : initiate a connection on a socket

    environ(Drv, ForkChain) -> [binary()]

        environ(7) : return the process environment variables

    event(Drv, ForkChain) -> term()

        event/1,2 is used to retrieve async messages returned from the
        port, such as caught signals, the exit status or the termination
        signal.

    execve(Drv, ForkChain, Arg0, [Arg0, Args], Env) -> ok | {error, posix()}

        Types   Arg0 = Args = iodata()
                Env = [iodata()]

        execve(2) : replace the process image, specifying the environment
        for the new process image.

    execvp(Drv, ForkChain, Arg0, [Arg0, Args]) -> ok | {error, posix()}

        Types   Arg0 = Args = iodata()
                Env = [iodata()]

        execvp(2) : replace the current process image using the search path

    exit(Drv, ForkChain, Value) -> ok

        Types   Value = integer()

        exit(3) : cause the child process to exit

    fcntl(Drv, ForkChain, FD, Cmd, Arg) -> {ok,int64_t()} | {error, posix()}.

        Types   FD = int32_t()
                Cmd = int32_t()
                Arg = int64_t()

        fcntl(2) : perform operations on a file descriptor

    file_constant(Drv, ForkChain, atom()) -> integer() | unknown

        Constants for open(2).

    fork(Drv, ForkChain) -> {ok, integer()} | {error, posix()}

        fork(2) : create a new process

    getcwd(Drv, ForkChain) -> {ok, binary()} | {error, posix()}

        getcwd(3) : return the current working directory

    getenv(Drv, ForkChain, iodata()) -> binary() | false

        getenv(3) : retrieve an environment variable

    getgid(Drv, ForkChain) -> non_neg_integer()

        getgid(2) : retrieve the processes' group ID

    getgroups(Drv, ForkChain) -> {ok, [gid_t()]} | {error, posix()}

        getgroups(2) : retrieve the list of supplementary groups

    gethostname(Drv, ForkChain) -> {ok, binary()} | {error, posix()}

        gethostname(2) : retrieve the system hostname

    getopt(Drv, ForkChain, Options) -> integer() | false

        Types   Options = exit_status | maxchild | maxforkdepth | termsig

        Retrieve port options for event loop. These options are
        configurable per process, with the default settings inherited
        from the parent.

        The initial values for these options are set for the port by
        alcove_drv:start/1.

            maxchild : non_neg_integer() : (ulimit -n) / 4 - 4

                Number of child processes allowed for this process. This
                value can be modifed by adjusting RLIMIT_NOFILE for
                the process.

            exit_status : 1 | 0 : 0

                Controls whether the controlling Erlang process is
                informed of a process' exit value.

            maxforkdepth : non_neg_integer() : 16

                Sets the maximum length of the fork chain.

            termsig : 1 | 0 : 0

                If a child process exits because of a signal, notify
                the controlling Erlang process.

    getpgrp(Drv, ForkChain) -> integer()

        getpgrp(2) : retrieve the process group.

    getpid(Drv, ForkChain) -> integer()

        getpid(2) : retrieve the system PID of the process.

    getpriority(Drv, ForkChain, Which, Who) -> {ok, Prio} | {error, posix()}

        Types   Which = constant()
                Who = Prio = integer()

        getpriority(2) : retrieve scheduling priority of process,
        process group or user

    getresgid(Drv, ForkChain) -> {ok, RGID, EGID, SGID}

        Types   RGID = EGID = SGID = non_neg_integer()

        getresgid(2) : get real, effective and saved group ID

        Supported on Linux and BSD's.

    getresuid(Drv, ForkChain) -> {ok, RUID, EUID, SUID}

        Types   RUID = EUID = SUID = non_neg_integer()

        getresuid(2) : get real, effective and saved user ID

        Supported on Linux and BSD's.

    getrlimit(Drv, ForkChain, constant()) -> {ok, #alcove_rlimit{}} | {error, posix()}

        getrlimit(2) : retrive the resource limits for a process. Returns
        a record:

            -include_lib("alcove/include/alcove.hrl").

            #alcove_rlimit{
                cur = integer(),
                max = integer()
                }

    getsid(Drv, ForkChain, OSPid) -> {ok, integer()} | {error, posix()}

        getsid(2) : retrieve the session ID

    getuid(Drv, ForkChain) -> integer()

        getuid(2) : returns the process user ID

    ioctl(Drv, ForkChain, FD, Request, Argp) -> {ok, Result, Bin} | {error, posix()}

        Types   FD = int32_t()
                Request = int32_t()
                Argp = binary() | Cstruct
                Cstruct = [binary() | {ptr, non_neg_integer() | binary()}]
                Result = integer()
                Bin = binary()

        ioctl(2) : control device

        Controls a device using a file descriptor previously obtained
        using open/5.

        Argp can be either a binary or a list represention of a C
        struct. See prctl/7 below for a description of the list elements.

        On success, ioctl/5 returns a 3-tuple:

            Result: an integer equal to the return value of the ioctl.

                    Usually 0 but some ioctl's may use the return value as the
                    output parameter.

            Bin: the value depends on the type of the input parameter Argp.

                    cstruct: contains the contents of the memory pointed to by
                             Argp

                    integer/binary: an empty binary

        An example of creating a tap device in a net namespace on Linux:

            {ok, Child} = alcove:clone(Drv, [], [clone_newnet]),
            {ok, FD} = alcove:open(Drv, [Child], "/dev/net/tun", [o_rdwr], 0),
            TUNSETIFF = alcove_ioctl:iow($T, 202, 4),
            {ok, _, <<"tap", N, _/binary>>} = alcove:ioctl(Drv, [Child], FD,
                TUNSETIFF, <<
                0:(16*8), % generate a tuntap device name
                (16#0002 bor 16#1000):2/native-unsigned-integer-unit:8, % IFF_TAP, IFF_NO_PI
                0:(14*8)
                >>),
            {ok, <<"tap", N>>}.

    jail(Drv, ForkChain, Cstruct) -> {ok, int32_t()} | {error, posix()}

        Types   Cstruct = [binary() | {ptr, non_neg_integer() | binary()}]

        FreeBSD only.

        jail(2) : restrict the current process in a system jail

        An example of a function to generate a version 2 jail struct
        (FreeBSD 10.2):

            struct_jail2(Path, Hostname, Jailname, IPv4, IPv6) ->
                [<<2:4/native-unsigned-integer-unit:8>>,
                 <<0:(alcove:wordalign(4) * 8)>>,
                 {ptr, <<Path/binary, 0>>},
                 {ptr, <<Hostname/binary, 0>>},
                 {ptr, <<Jailname/binary, 0>>},
                 <<(length(IPv4)):4/native-unsigned-integer-unit:8,
                   (length(IPv6)):4/native-unsigned-integer-unit:8>>,
                 {ptr, << <<IP1,IP2,IP3,IP4>> || {IP1,IP2,IP3,IP4} <- IPv4 >>},
                 {ptr, << <<IP1:16,IP2:16,IP3:16,IP4:16,IP5:16,IP6:16,IP7:16,IP8:16>>
                          || {IP1,IP2,IP3,IP4,IP5,IP6,IP7,IP8} <- IPv6 >>}].

        To apply the jail:

            {ok, Child} = alcove:fork(Drv, []),
            Jailv2 = struct_jail2(<<"/rescue">>, <<"test">>, <<"jail0">>, [], []),
            {ok, JID} = alcove:jail(Drv, [Child], Jailv2),
            ok = alcove:chdir(Drv, [Child], "/").

    jail_attach(Drv, ForkChain, int32_t()) -> ok | {error, posix()}

        FreeBSD only.

        jail_attach(2) : join a jailed process

    jail_remove(Drv, ForkChain, int32_t()) -> ok | {error, posix()}

        FreeBSD only.

        jail_remove(2) : destroy a jailed process

    kill(Drv, ForkChain, OSPid, Signal) -> ok | {error, posix()}

        Types   Signal = constant()

        kill(2) : terminate a process

    link(Drv, ForkChain, Oldpath, Newpath) -> ok | {error, posix()}

        Types   Oldpath = Newpath = iodata()

        link(2) : create a hard link

    lseek(Drv, ForkChain, FD, Offset, Whence) -> ok | {error, posix()}

        Types   Offset = Whence = integer()

        lseek(2) : set file offset for read/write

    mkdir(Drv, ForkChain, Path, Mode) -> ok | {error, posix()}

        mkdir(2) : create a directory

    mount(Drv, ForkChain, Source, Target, FSType, Flags, Data, Options) -> ok
        | {error, posix()}

        Types   Source = Target = FSType = Data = Options = iodata()
                Flags = integer() | [constant()]

        mount(2) : mount a filesystem, Linux style

        The arguments are:

            * source
            * target
            * filesystem type
            * flags
            * data

        An empty binary may be used to specify NULL.

        For example, filesystems mounted in a Linux mount namespace may
        be visible in the global mount namepace. To avoid this, first
        remount the root filesystem within mount namespace using the
        MS_REC|MS_PRIVATE flags:

            {ok, Task} = prx:clone(Parent, [clone_newns]),
            ok = prx:mount(Task, "none", "/", <<>>, [ms_rec, ms_private], <<>>).

        On BSD systems, the Source argument is ignored and passed to
        the system mount call as:

            mount(FSType, Target, Flags, Data);

        On Solaris, some mount options are passed in the Options argument
        as a string of comma separated values terminated by a NULL.
        Other platforms ignore the Options parameter.

    mount_constant(Drv, ForkChain, Flag) -> integer() | unknown

        Types   Flag = rdonly | nosuid | noexec | noatime | ...

        Convert flag names to integers. The lower case atoms are used
        for portability:

            alcove:mount_constant(Drv, [], rdonly)

        'rdonly' is mapped to MS_RDONLY on Linux and MNT_RDONLY on
        FreeBSD.

    open(Drv, ForkChain, Path, Flags, Mode) -> {ok, integer()} | {error, posix()}

        Types   Flags = integer() | [constant()]
                Mode = integer()

        open(2) : returns a file descriptor associated with a file

        Lists of values are OR'ed:

            alcove:open(Drv, [], "/tmp/test", [o_wronly,o_creat], 8#644)

    pledge(Drv, ForkChain, Promises, Paths) -> ok | {error, posix()}

        Types	Promises = iodata()
              Paths = [string() | binary()]

        OpenBSD only.

        pledge(2) : restrict system operations

    prctl(Drv, ForkChain, Option, Arg2, Arg3, Arg4, Arg5) ->
        {ok, integer(), Val2, Val3, Val4, Val5} | {error, posix()}

        Types   Option = constant()
                Arg2 = Arg3 = Arg4 = Arg5 = constant() | binary() | Cstruct
                Val2 = Val3 = Val4 = Val5 = binary() | integer() | Cstruct
                Cstruct = [binary() | {ptr, non_neg_integer() | binary()}]

        Linux only.

        prctl(2) : operations on a process

        This function can be used to set BPF syscall filters on processes
        (seccomp mode).

        A list can be used for prctl operations requiring a C structure
        as an argument. List elements are used to contiguously populate
        a buffer (it is up to the caller to add padding):

            * binary(): the element is copied directly into the buffer

                        On return, the contents of the binary is returned
                        to the caller.

            * {ptr, N}: N bytes of memory is allocated and zero'ed. The
                        pointer is placed in the buffer.

                        On return, the contents of the memory is returned
                        to the caller.

            * {ptr, binary()}:

                        Memory equal to the size of the binary is
                        allocated and initialized with the contents of
                        the binary.

                        On return, the contents of the memory is returned
                        to the caller.

        For example, to enforce a seccomp filter:

                % NOTE: this filter will cause the port to receive a SIGSYS
                % See test/alcove_seccomp_tests.erl for all the syscalls
                % required for the port process to run

                Arch = alcove:define(Drv, [], alcove:audit_arch()),
                Filter = [
                    ?VALIDATE_ARCHITECTURE(Arch),
                    ?EXAMINE_SYSCALL,
                    sys_read,
                    sys_write
                ],

                {ok,_,_,_,_,_} = alcove:prctl(Drv, [], pr_set_no_new_privs, 1, 0, 0, 0),
                Pad = (erlang:system_info({wordsize,external}) - 2) * 8,

                Prog = [
                    <<(iolist_size(Filter) div 8):2/native-unsigned-integer-unit:8>>,
                    <<0:Pad>>,
                    {ptr, list_to_binary(Filter)}
                ],
                alcove:prctl(Drv, [], pr_set_seccomp, seccomp_mode_filter, Prog, 0, 0).


    prctl_constant(Drv, ForkChain, atom()) -> integer() | unknown

        Convert prctl option names to integers.

    read(Drv, ForkChain, Fd, Count) -> {ok, binary()} | {error, posix()}

        Types   Count = non_neg_integer()

        read(2) : read bytes from a file descriptor

    readdir(Drv, ForkChain, Path) -> {ok, [binary()]} | {error, posix()}

        readdir(3) : retrieve list of objects in a directory

    rlimit_constant(Drv, ForkChain, atom()) -> integer() | unknown

        Convert an RLIMIT_* flag to an integer().

    rmdir(Drv, ForkChain, Path) -> ok | {error, posix()}

        rmdir(2) : delete a directory

    seccomp(Drv, ForkChain, Operation, Flags, Prog) -> ok | {error, posix()}

        Types   Operation = constant()
                Flags = constant()
                Prog = [binary() | {ptr, non_neg_integer() | binary()}]

        seccomp(2) : restrict system operations

        See prctl/7.

    seccomp_constant(Drv, ForkChain, atom()) -> integer() | unknown

        Convert seccomp option names to integers.

    select(Drv, ForkChain, Readfds, Writefds, Exceptfds, Timeout) -> {ok, Readfds, Writefds, Exceptfds} | {error, posix()}

        Types   Readfds = Writefds = Exceptfds = [] | [integer()]
                Timeout = <<>> | #alcove_timeval{}

        select(2) : poll a list of file descriptor for events

        select/6 will block until an event occurs on a file descriptor,
        a timeout is reached or interrupted by a signal.

        The Timeout value may be:

            * an empty binary (<<>>) signifying no value (block forever)

            * an alcove_timeval record with these fields:

                sec : number of seconds to wait
                usec : number of microseconds to wait

    setenv(Drv, ForkChain, Name, Value, Overwrite) -> ok | {error, posix()}

        Types   Name = Value = iodata()
                Overwrite = 0 | 1

        setenv(3) : set an environment variable

    setgid(Drv, ForkChain, Gid) -> ok | {error, posix()}

        Types   Gid = non_neg_integer()

        setgid(2) : set the GID of the process

    setgroups(Drv, ForkChain, [Gid]) -> ok | {error, posix()}

        Types   Gid = non_neg_integer()

        setgroups(2) : set the supplementary groups of the process

    setpgid(Drv, ForkChain, OSPid, Pgid) -> ok | {error, posix()}

        Types   Pgid = integer()

        setpgid(2) : set process group

    setsid(Drv, ForkChain) -> {ok, OSPid} | {error, posix()}

        setsid(2) : create a new session

    sethostname(Drv, ForkChain, Hostname) -> ok | {error, posix()}

        Types   Hostname = iodata()

        sethostname(2) : set the system hostname

        This function is probably only useful if running in a uts namespace:

            {ok, Child} = alcove:clone(Drv, [], [clone_newuts]),
            ok = alcove:sethostname(Drv, [Child], "test"),
            Hostname1 = alcove:gethostname(Drv, []),
            Hostname2 = alcove:gethostname(Drv, [Child]),
            Hostname1 =/= Hostname2.

    setns(Drv, ForkChain, FD, NSType) -> ok | {error, posix()}

        Types   FD = int32_t()
                NSType = constant()

        Linux only.

        setns(2) : attach to a namespace

        A process namespace is represented as a path in the /proc
        filesystem. The path is /proc/<pid>/ns/<ns>, where:

            pid = the system PID
            ns = a file representing the namespace

        The available namespaces is dependent on the kernel version. You
        can see which are supported by running:

            ls -al /proc/$$/ns

        On Ubuntu 12.04, the ipc, net and uts namespaces are available.

        For example, to attach to another process' network namespace:

            {ok, Child1} = alcove:clone(Drv, [], [clone_newnet]),
            {ok, Child2} = alcove:fork(Drv, []),

            % Move Child2 into the Child1 network namespace
            {ok,FD} = alcove:open(Drv, [Child2],
                    ["/proc/", integer_to_list(Child1), "/ns/net"], [o_rdonly], 0),
            ok = alcove:setns(Drv, [Child2], FD, 0),
            ok = alcove:close(Drv, [Child2], FD).

    setopt(Drv, ForkChain, Opt, Val) -> boolean()

        Set port options. See getopt/2,3 for the list of options.

    setpriority(Drv, ForkChain, Which, Who, Prio) -> ok | {error, posix()}

        Types   Which = constant()
                Who = Prio = integer()

        setpriority(2) : set scheduling priority of process, process
        group or user

    setproctitle(Drv, ForkChain, Name) -> ok

        Types   Name = iodata()

        BSD only.

        setproctitle(3) : set the process title

        On Linux, use prctl/6,7:

            {ok,Fork} = alcove:fork(Drv, []),
            alcove:prctl(Drv, [Fork], pr_set_name, <<"pseudonym">>, 0,0,0).

    setresgid(Drv, ForkChain, RGID, EGID, SGID) -> ok | {error, posix()}

        Types   RGID = EGID = SGID = non_neg_integer()

        setresgid(2) : set real, effective and saved group ID

        Supported on Linux and BSD's.

    setresuid(Drv, ForkChain, RUID, EUID, SUID) -> ok | {error, posix()}

        Types   RUID = EUID = SUID = non_neg_integer()

        setresuid(2) : set real, effective and saved user ID

        Supported on Linux and BSD's.

    setrlimit(Drv, ForkChain, Resource, Limit) -> ok | {error, posix()}

        Types   Resource = constant()
                Val = #alcove_rlimit{}

        setrlimit(2) : set a resource limit

    setuid(Drv, ForkChain, UID) -> ok | {error, posix()}

        Types   UID = non_neg_integer()

        setuid(2) : change UID

    sigaction(Drv, ForkChain, Signum, Handler) -> {ok, OldHandler} | {error, posix()}

        Types   Signum = constant()
                Handler = sig_dfl | sig_ign | sig_info | <<>>
                OldHandler = sig_dfl | sig_ign | sig_info

        sigaction(2) : set process behaviour for signals

            sig_dfl : uses the default behaviour for the signal

            sig_ign : ignores the signal

            sig_info : catches the signal and sends the controlling Erlang
                       process an event: {signal, atom(), Info}

                       'Info' is a binary containing the siginfo_t
                       structure. See sigaction(2) for details.

            <<>> : returns the current handler for the signal

        Multiple caught signals of the same type may be reported as one event.

    signal_constant(Drv, ForkChain, atom()) -> integer() | unknown

        Convert signal names to integers.

    socket(Drv, ForkChain, Domain, Type, Protocol) -> {ok, fd()} | {error, posix()}

        Types   Domain = Type = constant()
                Protocol = int32_t()

        socket(2) : returns a file descriptor for a communication endpoint

    symlink(Drv, ForkChain, Oldpath, Newpath) -> ok | {error, posix()}

        Types   Oldpath = Newpath = iodata()

        symlink(2) : create a symbolic link

    umount(Drv, ForkChain, Path) -> ok | {error, posix()}

        umount(2) : unmount a filesystem

        On BSD systems, calls unmount(2).

    unsetenv(Drv, ForkChain, Name) -> ok | {error, posix()}

        unsetenv(3) : remove an environment variable

    unshare(Drv, ForkChain, Flags) -> ok | {error, posix()}

        Types   Flags = constant()

        Linux only.

        unshare(2) : allows creating a new namespace in the current process

        unshare(2) lets you make a new namespace without calling clone(2):

            ok = alcove:unshare(Drv, [], [clone_newnet]).

            % The port is now running in a namespace without network access.

    version(Drv, ForkChain) -> binary()

        Retrieves the alcove version.

    write(Drv, ForkChain, FD, Buf) -> {ok, Count} | {error, posix()}

        Types   Buf = iodata()
                Count = non_neg_integer()

        Writes a buffer to a file descriptor and returns the number of
        bytes written.

The alcove module functions accept an additional argument which allows
setting timeouts. For example:

    write(Drv, ForkChain, FD, Buf) -> {ok, Count} | {error, posix()}
    write(Drv, ForkChain, FD, Buf, timeout()) -> {ok, Count} | {error, posix()}

By default, timeout is set to infinity. Similar to gen_server:call/3,
setting an integer timeout will cause the process to crash if the
timeout is reached. If the failure is caught, the caller must deal with
any delayed messages that arrive for the Unix process described by the
fork chain.

See "Message Format" for a description of the messages.

### Standard I/0

These functions handle stdin, stdout and stderr for the processs after
exec(3) has been called.

    stdin(Drv, ForkChain, Buf) -> ok

        Types   Buf = iodata()

        Send data to stdin of the process.

    stdout(Drv, ForkChain) -> binary() | false

        Read stdout from the process.

    stderr(Drv, ForkChain) -> binary() | false

        Read stderr from the process.

    eof(Drv, ForkChain) -> ok | {error, posix()}

        Close stdin of the process.

Message Format
==============

* synchronous replies to calls from alcove processes running in the
  event loop. The type of the last element of the tuple depends on the call
  (e.g., open/4,5 would return either {ok, integer()} or {error, posix()}

        {alcove_call, pid(), [non_neg_integer()], term()}

* asynchronous events generated by the alcove process (e.g., signals).

        {alcove_event, pid(), [non_neg_integer()], term()}

* standard error: can be generated by an alcove process running in the
  event loop as well as a Unix process.

        {alcove_stderr, pid(), [non_neg_integer()], binary()}

* standard output: output from the Unix process after alcove has called
  exec(3)

        {alcove_stdout, pid(), [non_neg_integer()], binary()}

Examples
========

To compile the examples:

```
make eg
```

* GPIO

`examples/gpioled.erl` is a simple example of interacting with the GPIO
on a beaglebone black or raspberry pi that will blink an LED. The example
works with two system processes:

    * a port process which requests the GPIO pin be exported to user
      space, forks a child into a new namespace, then drops privileges

    * a child process gets a file descriptor for the GPIO, then drops
      privileges

Tests
=====

    $ rebar3 ct
