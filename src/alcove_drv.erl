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
-behaviour(gen_server).
-include_lib("alcove/include/alcove.hrl").

%% API
-export([start/0, start/1, stop/1]).
-export([start_link/2]).
-export([call/5]).
-export([stdin/3, stdout/3, stderr/3, event/3, send/3]).
-export([getopts/1]).

%% gen_server callbacks
-export([init/1, handle_call/3, handle_cast/2, handle_info/2,
         terminate/2, code_change/3]).

-type ref() :: pid().
-export_type([ref/0]).

-record(state, {
        pid :: pid(),
        port :: port(),
        caller = dict:new() :: dict:dict(),
        buf = <<>>
    }).

-spec start() -> {ok, ref()}.
start() ->
    start_link(self(), []).

-spec start(proplists:proplist()) -> {ok, ref()}.
start(Options) ->
    start_link(self(), Options).

-spec start_link(pid(), proplists:proplist()) -> {ok, ref()}.
start_link(Owner, Options) ->
    gen_server:start_link(?MODULE, [Owner, Options], []).

-spec stop(ref()) -> ok.
stop(Drv) ->
    gen_server:call(Drv, stop).

-spec call(ref(),[integer()],atom(),list(),timeout()) -> term().
call(Drv, Pids, Command, Argv, Timeout) ->
    Data = alcove_codec:call(Command, Pids, Argv),
    case send(Drv, Pids, Data) of
        true ->
            call_reply(Drv, Pids, alcove_proto:returns(Command), Timeout);
        Error ->
            Error
    end.

-spec send(ref(),[integer()],iodata()) -> true | {error,closed} | badarg.
send(Drv, Pids, Data) ->
    case iolist_size(Data) =< 16#ffff of
        true ->
            gen_server:call(Drv, {send, Pids, Data}, infinity);
        false ->
            badarg
    end.

-spec stdin(ref(),[integer()],iodata()) -> 'true'.
stdin(Drv, Pids, Data) ->
    Stdin = alcove_codec:stdin(Pids, Data),
    send(Drv, Pids, Stdin).

-spec stdout(ref(),[integer()],timeout()) -> 'false' | binary().
stdout(Drv, Pids, Timeout) ->
    reply(Drv, Pids, alcove_stdout, Timeout).

-spec stderr(ref(),[integer()],timeout()) -> 'false' | binary().
stderr(Drv, Pids, Timeout) ->
    reply(Drv, Pids, alcove_stderr, Timeout).

-spec event(ref(),[integer()],timeout()) -> term().
event(Drv, Pids, Timeout) ->
    reply(Drv, Pids, alcove_event, Timeout).

%%--------------------------------------------------------------------
%%% Callbacks
%%--------------------------------------------------------------------
init([Owner, Options]) ->
    process_flag(trap_exit, true),

    [Cmd|Argv] = getopts(Options),
    PortOpt = lists:filter(fun
            (stderr_to_stdout) -> true;
            ({env,_}) -> true;
            (_) -> false
        end, Options),

    Port = open_port({spawn_executable, Cmd}, [
            {args, Argv},
            stream,
            binary
        ] ++ PortOpt),

    {ok, #state{port = Port, pid = Owner}}.

handle_call({send, OSPids, Packet}, {Pid,_Tag}, #state{port = Port, caller = Caller} = State) ->
    case is_monitored(Pid) of
        true -> ok;
        false -> monitor(process, Pid)
    end,
    Reply = try erlang:port_command(Port, Packet) of
        true ->
            true
        catch
            error:badarg ->
                {error,closed}
        end,
    {reply, Reply, State#state{caller = dict:store(OSPids, Pid, Caller)}};

handle_call(stop, _From, State) ->
    {stop, normal, ok, State}.

handle_cast(_Msg, State) ->
    {noreply, State}.

terminate(_Reason, #state{port = Port}) ->
    catch erlang:port_close(Port),
    ok.

code_change(_OldVsn, State, _Extra) ->
    {ok, State}.

%%--------------------------------------------------------------------
%%% Port communication
%%--------------------------------------------------------------------

% Reply from a child process.
%
% Several writes from the child process may be coalesced into 1 read by
% the parent.
handle_info({Port, {data, Data}}, #state{port = Port, pid = Pid, buf = Buf, caller = Caller} = State) ->
    {Msgs, Rest} = alcove_codec:stream(<<Buf/binary, Data/binary>>),
    Terms = [ alcove_codec:decode(Msg) || Msg <- Msgs ],
    [ get_value(Pids, Caller, Pid) ! {Tag, self(), Pids, Term}
        || {Tag, Pids, Term} <- Terms ],
    {noreply, State#state{buf = Rest}};

handle_info({'DOWN', _MonitorRef, _Type, Pid, _Info}, #state{caller = Caller} = State) ->
    {noreply, State#state{
            caller = dict:filter(fun(_K,V) -> V =/= Pid end, Caller)
        }};

handle_info({'EXIT', Port, Reason}, #state{port = Port} = State) ->
    {stop, {shutdown, Reason}, State};

% WTF
handle_info(Info, State) ->
    error_logger:error_report([{wtf, Info}]),
    {noreply, State}.

%%--------------------------------------------------------------------
%%% Internal functions
%%--------------------------------------------------------------------
call_reply(Drv, Pids, false, Timeout) ->
    receive
        {alcove_event, Drv, Pids, fdctl_closed} ->
            ok;
        {alcove_call, Drv, Pids, Event} ->
            Event
    after
        Timeout ->
            exit(timeout)
    end;
call_reply(Drv, Pids, true, Timeout) ->
    receive
        {alcove_event, Drv, Pids, {termsig,_} = Signal} ->
            exit(Signal);
        {alcove_event, Drv, Pids, fdctl_closed} ->
            receive
                {alcove_event, Drv, Pids, {termsig,_} = Signal} ->
                    exit(Signal);
                {alcove_event, Drv, Pids, {exit_status,_} = Status} ->
                    exit(Status)
            end;
        {alcove_call, Drv, Pids, Event} ->
            Event
    after
        Timeout ->
            exit(timeout)
    end.

reply(Drv, Pids, Type, Timeout) ->
    receive
        {Type, Drv, Pids, Event} ->
            Event
    after
        Timeout ->
            false
    end.

is_monitored(Pid) ->
    {monitored_by, Monitors} = process_info(Pid, monitored_by),
    lists:member(self(), Monitors).

get_value(Key, Dict, Default) ->
    case dict:find(Key, Dict) of
        error -> Default;
        {ok,Val} -> Val
    end.

%%--------------------------------------------------------------------
%%% Port executable
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
optarg({maxforkdepth, Arg})     -> switch("M", Arg);
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
