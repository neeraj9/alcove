%%% Copyright (c) 2014, Michael Santos <michael.santos@gmail.com>
%%%
%%% Permission to use, copy, modify, and/or distribute this software for any
%%% purpose with or without fee is hereby granted, provided that the above
%%% copyright notice and this permission notice appear in all copies.
%%%
%%% THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
%%% WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
%%% MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
%%% ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
%%% WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
%%% ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
%%% OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
-module(alcove_drv).
-include_lib("alcove/include/alcove.hrl").

%% API
-export([start/0, start/1, stop/1]).
-export([call/2, call/3, call/4, cast/2, encode/2, encode/3]).
-export([stdin/3, stdout/3, stderr/3, event/4]).
-export([atom_to_type/1, type_to_atom/1]).
-export([msg/2, events/4]).
-export([getopts/1]).

-export_type([reply/0]).

-type prctl_val() :: binary() | non_neg_integer().

-type reply() :: 'badarg' | 'ok' | boolean() | binary()
    | non_neg_integer() | [integer()]
    | {'ok', binary() | non_neg_integer() | #rlimit{} | 'unsupported'}
    | {'error', file:posix()}
    | {'ok',integer(),prctl_val(), prctl_val(), prctl_val(), prctl_val()}.

-spec start() -> port().
start() ->
    start([]).

-spec start(proplists:proplist()) -> port().
start(Options) ->
    [Cmd|Argv] = getopts(Options),
    open_port({spawn_executable, Cmd}, [{args, Argv}, {packet, 2}, binary]).

-spec call(port(),iodata()) -> reply().
call(Port, Data) ->
    call(Port, [], Data, 5000).

-spec call(port(),[integer()],iodata()) -> reply().
call(Port, Pids, Data) ->
    call(Port, Pids, Data, 5000).

-spec call(port(),[integer()],iodata(),'infinity' | non_neg_integer()) ->
    reply().
call(Port, Pids, Data, Timeout) ->
    true = send(Port, Data, iolist_size(Data)),
    case event(Port, Pids, ?ALCOVE_MSG_CALL, Timeout) of
        false ->
            false;
        {alcove_call, Pids, Event} ->
            Event
    end.

-spec cast(port(),iodata()) -> any().
cast(Port, Data) ->
    send(Port, Data, iolist_size(Data)).

-spec send(port(),iodata(),pos_integer()) -> any().
send(Port, Data, Size) when is_port(Port), Size < 16#ffff ->
    erlang:port_command(Port, Data).

-spec event(port(),[integer()],non_neg_integer(),
    'infinity' | non_neg_integer()) -> 'false' |
    {'alcove_call' | 'alcove_event',
        [integer()], reply() | {'signal', integer()}}.
% Check the mailbox for processed events
event(Port, Pids, Type, Timeout) when is_integer(Type) ->
    Tag = type_to_atom(Type),
    receive
        {Port, {Tag, Pids, _Data} = Event} ->
            Event
    after
        0 ->
            event_1(Port, Pids, Type, Timeout)
    end.

% Check for messages from the port

% Reply from the port: no message length
event_1(Port, [], Type, Timeout) ->
    receive
        {Port, {data, <<?UINT16(Type), Reply/binary>>}} ->
            {type_to_atom(Type), [], binary_to_term(Reply)}
    after
        Timeout ->
            false
    end;

% Reply from a child process.
%
% The parent process may coalesce 2 writes from the child into 1 read. The
% parent could read the length header then read length bytes except that
% the child may have called execvp(). After calling exec(), the data
% returned from the child will not contain a length header.
%
% Work around this by converting the reply into a list of messages. The
% first message matching the requested type is returned to the caller. The
% remaining messages are pushed back into the process' mailbox.
event_1(Port, [Pid0] = Pids, Type, Timeout) ->
    receive
        {Port, {data, <<
                ?ALCOVE_HDR(?ALCOVE_MSG_PROXY, Pid0),
                ?UINT16(Len), ?UINT16(Type), Reply/binary
            >>}} when Len =:= 2 + byte_size(Reply) ->
            {type_to_atom(Type), Pids, binary_to_term(Reply)};
        {Port, {data, <<
                ?ALCOVE_HDR(?ALCOVE_MSG_PROXY, Pid0),
                ?UINT16(Len), ?UINT16(Type1), Reply/binary
            >>}} when Len =:= 2 + byte_size(Reply),
            Type1 =:= ?ALCOVE_MSG_CALL orelse Type1 =:= ?ALCOVE_MSG_EVENT ->
            events(Port, Pids, Type,
                <<?UINT16(Len), ?UINT16(Type1), Reply/binary>>),
            event_1(Port, Pids, Type, Timeout);
        {Port, {data, <<
                ?ALCOVE_HDR(?ALCOVE_MSG_PROXY, Pid0),
                ?UINT16(Len), ?UINT16(Type1), Reply/binary
            >>}} when Type1 =:= ?ALCOVE_MSG_CALL
                orelse Type1 =:= ?ALCOVE_MSG_EVENT ->
            events(Port, Pids, Type,
                <<?UINT16(Len), ?UINT16(Type1), Reply/binary>>)
    after
        Timeout ->
            false
    end;
event_1(Port, [Pid0,Pid1] = Pids, Type, Timeout) ->
    receive
        {Port, {data, <<
                ?ALCOVE_HDR(?ALCOVE_MSG_PROXY, Pid0, Pid1),
                ?UINT16(Len), ?UINT16(Type), Reply/binary
            >>}} when Len =:= 2 + byte_size(Reply) ->
            {type_to_atom(Type), Pids, binary_to_term(Reply)};
        {Port, {data, <<
                ?ALCOVE_HDR(?ALCOVE_MSG_PROXY, Pid0, Pid1),
                ?UINT16(Len), ?UINT16(Type1), Reply/binary
            >>}} when Len =:= 2 + byte_size(Reply),
            Type1 =:= ?ALCOVE_MSG_CALL orelse Type1 =:= ?ALCOVE_MSG_EVENT ->
            events(Port, Pids, Type,
                <<?UINT16(Len), ?UINT16(Type1), Reply/binary>>),
            event_1(Port, Pids, Type, Timeout);
        {Port, {data, <<
                ?ALCOVE_HDR(?ALCOVE_MSG_PROXY, Pid0, Pid1),
                ?UINT16(Len), ?UINT16(Type1), Reply/binary
            >>}} when Type1 =:= ?ALCOVE_MSG_CALL
                orelse Type1 =:= ?ALCOVE_MSG_EVENT ->
            events(Port, Pids, Type,
                <<?UINT16(Len), ?UINT16(Type1), Reply/binary>>)
    after
        Timeout ->
            false
    end;
event_1(Port, [Pid0,Pid1,Pid2] = Pids, Type, Timeout) ->
    receive
        {Port, {data, <<
                ?ALCOVE_HDR(?ALCOVE_MSG_PROXY, Pid0, Pid1, Pid2),
                ?UINT16(Len), ?UINT16(Type), Reply/binary
            >>}} when Len =:= 2 + byte_size(Reply) ->
            {type_to_atom(Type), Pids, binary_to_term(Reply)};
        {Port, {data, <<
                ?ALCOVE_HDR(?ALCOVE_MSG_PROXY, Pid0, Pid1, Pid2),
                ?UINT16(Len), ?UINT16(Type1), Reply/binary
            >>}} when Len =:= 2 + byte_size(Reply),
            Type1 =:= ?ALCOVE_MSG_CALL orelse Type1 =:= ?ALCOVE_MSG_EVENT ->
            events(Port, Pids, Type,
                <<?UINT16(Len), ?UINT16(Type1), Reply/binary>>),
            event_1(Port, Pids, Type, Timeout);
        {Port, {data, <<
                ?ALCOVE_HDR(?ALCOVE_MSG_PROXY, Pid0, Pid1, Pid2),
                ?UINT16(Len), ?UINT16(Type1), Reply/binary
            >>}} when Type1 =:= ?ALCOVE_MSG_CALL
                orelse Type1 =:= ?ALCOVE_MSG_EVENT ->
            events(Port, Pids, Type,
                <<?UINT16(Len), ?UINT16(Type1), Reply/binary>>)
    after
        Timeout ->
            false
    end;
event_1(Port, [Pid0,Pid1,Pid2,Pid3] = Pids, Type, Timeout) ->
    receive
        {Port, {data, <<
                ?ALCOVE_HDR(?ALCOVE_MSG_PROXY, Pid0, Pid1, Pid2, Pid3),
                ?UINT16(Len), ?UINT16(Type), Reply/binary
            >>}} when Len =:= 2 + byte_size(Reply) ->
            {type_to_atom(Type), Pids, binary_to_term(Reply)};
        {Port, {data, <<
                ?ALCOVE_HDR(?ALCOVE_MSG_PROXY, Pid0, Pid1, Pid2, Pid3),
                ?UINT16(Len), ?UINT16(Type1), Reply/binary
            >>}} when Len =:= 2 + byte_size(Reply),
            Type1 =:= ?ALCOVE_MSG_CALL orelse Type1 =:= ?ALCOVE_MSG_EVENT ->
            events(Port, Pids, Type,
                <<?UINT16(Len), ?UINT16(Type1), Reply/binary>>),
            event_1(Port, Pids, Type, Timeout);
        {Port, {data, <<
                ?ALCOVE_HDR(?ALCOVE_MSG_PROXY, Pid0, Pid1, Pid2, Pid3),
                ?UINT16(Len), ?UINT16(Type1), Reply/binary
            >>}} when Type1 =:= ?ALCOVE_MSG_CALL
                orelse Type1 =:= ?ALCOVE_MSG_EVENT ->
            events(Port, Pids, Type,
                <<?UINT16(Len), ?UINT16(Type1), Reply/binary>>)
    after
        Timeout ->
            false
    end;
event_1(Port, [Pid0,Pid1,Pid2,Pid3,Pid4] = Pids, Type, Timeout) ->
    receive
        {Port, {data, <<
                ?ALCOVE_HDR(?ALCOVE_MSG_PROXY, Pid0, Pid1, Pid2, Pid3, Pid4),
                ?UINT16(Len), ?UINT16(Type), Reply/binary
            >>}} when Len =:= 2 + byte_size(Reply) ->
            {type_to_atom(Type), Pids, binary_to_term(Reply)};
        {Port, {data, <<
                ?ALCOVE_HDR(?ALCOVE_MSG_PROXY, Pid0, Pid1, Pid2, Pid3, Pid4),
                ?UINT16(Len), ?UINT16(Type1), Reply/binary
            >>}} when Len =:= 2 + byte_size(Reply),
            Type1 =:= ?ALCOVE_MSG_CALL orelse Type1 =:= ?ALCOVE_MSG_EVENT ->
            events(Port, Pids, Type,
                <<?UINT16(Len), ?UINT16(Type1), Reply/binary>>),
            event_1(Port, Pids, Type, Timeout);
        {Port, {data, <<
                ?ALCOVE_HDR(?ALCOVE_MSG_PROXY, Pid0, Pid1, Pid2, Pid3, Pid4),
                ?UINT16(Len), ?UINT16(Type1), Reply/binary
            >>}} when Type1 =:= ?ALCOVE_MSG_CALL
                orelse Type1 =:= ?ALCOVE_MSG_EVENT ->
            events(Port, Pids, Type,
                <<?UINT16(Len), ?UINT16(Type1), Reply/binary>>)
    after
        Timeout ->
            false
    end.

events(Port, Pids, Type, Reply) ->
    events(Port, Pids, Type, Reply, []).

events(Port, _Pids, ReqType, <<>>, Acc0) ->
    Tag = type_to_atom(ReqType),
    Acc = lists:reverse(Acc0),
    Event = lists:keyfind(Tag, 1, Acc),
    Events = lists:keydelete(Tag, 1, Acc),
    Self = self(),
    [ Self ! {Port, E} || E <- Events ],
    case Event of
        false ->
            false;
        Event ->
            Event
    end;
events(Port, Pids, ReqType,
    <<?UINT16(Len), ?UINT16(Type), Reply/binary>>, Acc) ->
    % length includes the message type field
    Bytes = Len - 2,
    <<Bin:Bytes/binary, Rest/binary>> = Reply,
    events(Port, Pids, ReqType, Rest,
        [{type_to_atom(Type), Pids, binary_to_term(Bin)}|Acc]).

-spec stdin(port(),[integer()],iodata()) -> 'true'.
stdin(Port, [], Data) ->
    cast(Port, Data);
stdin(Port, Pids, Data) ->
    Stdin = hdr(lists:reverse(Pids), [Data]),
    cast(Port, Stdin).

-spec stdout(port(),[integer()],'infinity' | non_neg_integer()) ->
    'false' | binary().
stdout(Port, Pids, Timeout) ->
    stdio(Port, Pids, ?ALCOVE_MSG_STDOUT, Timeout).

-spec stderr(port(),[integer()],'infinity' | non_neg_integer()) ->
    'false' | binary().
stderr(Port, Pids, Timeout) ->
    stdio(Port, Pids, ?ALCOVE_MSG_STDERR, Timeout).

-spec stdio(port(),[integer()],integer(),'infinity' | non_neg_integer()) ->
    'false' | binary().
stdio(Port, [], _Type, Timeout) ->
    receive
        {Port, {data, <<
                Reply/binary
                >>}} ->
            Reply
    after
        Timeout ->
            false
    end;
stdio(Port, [Pid0], Type, Timeout) ->
    receive
        {Port, {data, <<
                ?ALCOVE_HDR(Type, Pid0),
                Reply/binary
                >>}} ->
            Reply
    after
        Timeout ->
            false
    end;
stdio(Port, [Pid0, Pid1], Type, Timeout) ->
    receive
        {Port, {data, <<
                ?ALCOVE_HDR(Type, Pid0, Pid1),
                Reply/binary
                >>}} ->
            Reply
    after
        Timeout ->
            false
    end;
stdio(Port, [Pid0, Pid1, Pid2], Type, Timeout) ->
    receive
        {Port, {data, <<
                ?ALCOVE_HDR(Type, Pid0, Pid1, Pid2),
                Reply/binary
                >>}} ->
            Reply
    after
        Timeout ->
            false
    end;
stdio(Port, [Pid0, Pid1, Pid2, Pid3], Type, Timeout) ->
    receive
        {Port, {data, <<
                ?ALCOVE_HDR(Type, Pid0, Pid1, Pid2, Pid3),
                Reply/binary
                >>}} ->
            Reply
    after
        Timeout ->
            false
    end;
stdio(Port, [Pid0, Pid1, Pid2, Pid3, Pid4], Type, Timeout) ->
    receive
        {Port, {data, <<
                ?ALCOVE_HDR(Type, Pid0, Pid1, Pid2, Pid3, Pid4),
                Reply/binary
                >>}} ->
            Reply
    after
        Timeout ->
            false
    end.

msg([], Data) ->
    Data;
msg(Pids, Data) ->
    Size = iolist_size(Data),
    hdr(lists:reverse(Pids), [<<?UINT16(Size)>>, Data]).

hdr([], [_Length|Acc]) ->
    Acc;
hdr([Pid|Pids], Acc) ->
    Size = iolist_size(Acc) + 2 + 4,
    hdr(Pids, [<<?UINT16(Size)>>, <<?UINT16(?ALCOVE_MSG_STDIN)>>, <<?UINT32(Pid)>>|Acc]).

encode(Command, Arg) when is_integer(Command), is_list(Arg) ->
    encode(?ALCOVE_MSG_CALL, Command, Arg).
encode(Type, Command, Arg) when is_integer(Type), is_integer(Command), is_list(Arg) ->
    <<?UINT16(Type), ?UINT16(Command), (term_to_binary(Arg))/binary>>.

stop(Port) when is_port(Port) ->
    erlang:port_close(Port).

%%--------------------------------------------------------------------
%%% Internal functions
%%--------------------------------------------------------------------
-spec getopts(proplists:proplist()) -> list(string() | [string()]).
getopts(Options) when is_list(Options) ->
    Exec = proplists:get_value(exec, Options, ""),
    Progname = proplists:get_value(progname, Options, progname()),

    Options1 = lists:map(fun
                    (verbose) ->
                        {verbose, 1};
                    (N) when is_atom(N) ->
                        {N, true};
                    ({_,_} = N) ->
                        N
                end, Options),

    Switches = lists:append([ optarg(N) || N <- Options1 ]),
    [Cmd|Argv] = [ N || N <- string:tokens(Exec, " ") ++ [Progname|Switches], N /= ""],
    [find_executable(Cmd)|Argv].

optarg({verbose, Arg})          -> switch(string:copies("v", Arg));
optarg({maxchild, Arg})         -> switch("m", Arg);
optarg(_)                       -> "".

switch(Switch) ->
    [lists:concat(["-", Switch])].

switch(Switch, Arg) when is_binary(Arg) ->
    switch(Switch, binary_to_list(Arg));
switch(Switch, Arg) ->
    [lists:concat(["-", Switch, " ", Arg])].

find_executable(Exe) ->
    case os:find_executable(Exe) of
        false ->
            erlang:error(badarg, [Exe]);
        N ->
            N
    end.

atom_to_type(alcove_call) -> ?ALCOVE_MSG_CALL;
atom_to_type(alcove_event) -> ?ALCOVE_MSG_EVENT;
atom_to_type(alcove_stdin) -> ?ALCOVE_MSG_STDIN;
atom_to_type(alcove_stdout) -> ?ALCOVE_MSG_STDOUT;
atom_to_type(alcove_stderr) -> ?ALCOVE_MSG_STDERR;
atom_to_type(alcove_proxy) -> ?ALCOVE_MSG_PROXY.

type_to_atom(?ALCOVE_MSG_CALL) -> alcove_call;
type_to_atom(?ALCOVE_MSG_EVENT) -> alcove_event;
type_to_atom(?ALCOVE_MSG_STDIN) -> alcove_stdin;
type_to_atom(?ALCOVE_MSG_STDOUT) -> alcove_stdout;
type_to_atom(?ALCOVE_MSG_STDERR) -> alcove_stderr;
type_to_atom(?ALCOVE_MSG_PROXY) -> alcove_proxy.

basedir(Module) ->
    case code:priv_dir(Module) of
        {error, bad_name} ->
            filename:join([
                filename:dirname(code:which(Module)),
                "..",
                "priv"
            ]);
        Dir ->
            Dir
        end.

progname() ->
    filename:join([basedir(alcove), "alcove"]).
