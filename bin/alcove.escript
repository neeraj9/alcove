#!/usr/bin/env escript

%%%
%%% Generate the alcove.erl file
%%%
main([]) ->
    File = "alcove.erl",
    Proto = "c_src/alcove_cmd.proto",
    main([File, Proto]);

main([File, Proto]) ->
    mkerl(File, Proto).

license() ->
    {{Year,_,_},{_,_,_}} = calendar:universal_time(),

    Date = integer_to_list(Year),

    License = [
" Copyright (c) " ++ Date ++ ", Michael Santos <michael.santos@gmail.com>",
" Permission to use, copy, modify, and/or distribute this software for any",
" purpose with or without fee is hereby granted, provided that the above",
" copyright notice and this permission notice appear in all copies.",
"",
" THE SOFTWARE IS PROVIDED \"AS IS\" AND THE AUTHOR DISCLAIMS ALL WARRANTIES",
" WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF",
" MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR",
" ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES",
" WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN",
" ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF",
" OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE."],

    erl_syntax:comment(License).

api(Proto) ->
    Calls = calls(Proto),


    % Generate the function
    Pattern = [],
    Body = erl_syntax:tuple([ erl_syntax:atom(N) || {N,_} <- Calls ]),
    Clause = erl_syntax:clause(Pattern, [], [Body]),
    [erl_syntax:function(erl_syntax:atom("api"), [Clause])].

mkerl(File, Proto) ->
    Module = erl_syntax:attribute(
            erl_syntax:atom(module),
            [erl_syntax:atom(filename:basename(File, ".erl"))]
            ),
    Includes = includes(["alcove.hrl"]),

    % Type specs
    Specs = erl_syntax:comment(["%__SPECS__%%"]),

    % Any hardcoded functions will be included here
    Static = erl_syntax:comment(["%__STATIC__%%"]),

    Calls = calls(Proto),

    % Generate the list of exports
    Comment_static = erl_syntax:comment([" Static functions"]),
    Exports_static = erl_syntax:attribute(erl_syntax:atom(export), [
                erl_syntax:list([
                    erl_syntax:arity_qualifier(erl_syntax:atom(Fun), erl_syntax:integer(Arity))
                        || {Fun, Arity} <- static_exports() ])
                ]),

    Comment_gen = erl_syntax:comment([" Generated functions"]),
    Exports_gen0 = erl_syntax:attribute(erl_syntax:atom(export), [
                erl_syntax:list([
                    erl_syntax:arity_qualifier(erl_syntax:atom(Fun), erl_syntax:integer(Arity+1))
                        || {Fun, Arity} <- Calls ])
                ]),

    Exports_gen1 = erl_syntax:attribute(erl_syntax:atom(export), [
                erl_syntax:list([
                    erl_syntax:arity_qualifier(erl_syntax:atom(Fun), erl_syntax:integer(Arity+2))
                        || {Fun, Arity} <- Calls ])
                ]),

    % Generate the functions
    Functions = [ begin
                    % name(Port, ...) -> alcove:call(Port, [], Fun, [...])
                    Arg = arg("Arg", Arity),

                    Pattern0 = [erl_syntax:variable("Port")|Arg],
                    Body0 = erl_syntax:application(
                        erl_syntax:atom(call),
                        [erl_syntax:variable("Port"), erl_syntax:nil(),
                            erl_syntax:atom(Fun), erl_syntax:list(Arg)]
                    ),
                    Clause0 = erl_syntax:clause(Pattern0, [], [Body0]),

                    % name(Port, Pids, ...) -> alcove:call(Port, Pids, Fun, [...])
                    Pattern1 = [erl_syntax:variable("Port"), erl_syntax:variable("Pids")|Arg],
                    Body1 = erl_syntax:application(
                        erl_syntax:atom(call),
                        [erl_syntax:variable("Port"), erl_syntax:variable("Pids"),
                            erl_syntax:atom(Fun), erl_syntax:list(Arg)]
                    ),
                    Clause1 = erl_syntax:clause(Pattern1, [], [Body1]),

                    [erl_syntax:function(erl_syntax:atom(Fun), [Clause0]),
                        erl_syntax:function(erl_syntax:atom(Fun), [Clause1])]

                end || {Fun, Arity} <- Calls ],

    Code0 = erl_prettypr:format(erl_syntax:form_list(lists:flatten([
                license(),
                Module,
                Includes,

                Specs,

                Comment_static,
                Exports_static,

                Comment_gen,
                Exports_gen0,
                Exports_gen1,

                Static,
                api(Proto),
                Functions
            ]))),

    Code = lists:foldl(fun({Marker, Generated}, Text) ->
                re:replace(Text, Marker, Generated)
        end,
        Code0,
        [
            {"%%__STATIC__%%", static()},
            {"%%__SPECS__%%", specs()}
        ]),

%    io:format("~s~n", [Code]).
    file:write_file(File, [Code]).

arg(Prefix, Arity) ->
    [ erl_syntax:variable(string:concat(Prefix, integer_to_list(N))) || N <- lists:seq(1,Arity) ].

% List the supported alcove API functions
calls(Proto) ->
    {ok, Bin} = file:read_file(Proto),
    Fun = binary:split(Bin, <<"\n">>, [trim,global]),
    call_to_fun(Fun, []).

call_to_fun([], Acc) ->
    lists:reverse(Acc);
call_to_fun([H|T], Acc) ->
    [Fun, Arity] = binary:split(H, <<"/">>),
    Name = case Fun of
        <<"lxc_container_", Rest/binary>> ->
            Rest;
        _ ->
            Fun
    end,
    call_to_fun(T, [{binary_to_list(Name), binary_to_integer(Arity)}|Acc]).

static_exports() ->
    [{stdin,2}, {stdin,3},
     {stdout,2}, {stdout,3},
     {stderr,2}, {stderr,3},
     {ctl,1}, {ctl,2},
     {encode,2}, {encode,3},
     {command,1},
     {call,2},
     {call,3},
     {call,4}].

static() ->
    [ static({Fun, Arity}) || {Fun, Arity} <- static_exports() ].

static({stdin,2}) ->
"
stdin(Port, Data) ->
    stdin(Port, [], Data).
";
static({stdin,3}) ->
"
stdin(Port, Pids, Data) ->
    Stdin = alcove_drv:msg(Pids, Data),
    alcove_drv:cast(Port, Stdin).
";

static({stdout,2}) ->
"
stdout(Port, Pids) ->
    stdout(Port, Pids, 0).
";
static({stdout,3}) ->
"
% XXX discard all but the first PID
stdout(Port, [Pid|_], Timeout) ->
    receive
        {Port, {data, <<?UINT16(?ALCOVE_MSG_CHILDOUT), ?UINT32(Pid), Msg/binary>>}} ->
            Msg
    after
        Timeout ->
            false
    end.
";

static({stderr,2}) ->
"
stderr(Port, Pids) ->
    stderr(Port, Pids, 0).
";
static({stderr,3}) ->
"
% XXX discard all but the first PID
stderr(Port, [Pid|_], Timeout) ->
    receive
        {Port, {data, <<?UINT16(?ALCOVE_MSG_CHILDERR), ?UINT32(Pid), Msg/binary>>}} ->
            Msg
    after
        Timeout ->
            false
    end.
";

static({ctl,1}) ->
"
ctl(Port) ->
    ctl(Port, 0).
";
static({ctl,2}) ->
"
ctl(Port, Timeout) ->
    receive
        {Port, {data, <<?UINT16(?ALCOVE_MSG_CALL), Msg/binary>>}} ->
            binary_to_term(Msg)
    after
        Timeout ->
            false
    end.
";

static({encode,2}) ->
"
encode(Call, Arg) when is_atom(Call) ->
    encode(Call, [], Arg).
";

static({encode,3}) ->
"
encode(Call, Pids, Arg) when is_atom(Call), is_list(Pids), is_list(Arg) ->
    Bin = alcove_drv:encode(command(Call), Arg),
    alcove_drv:msg(Pids, Bin).
";

static({command,1}) ->
"
command(Cmd) when is_atom(Cmd) ->
    lookup(Cmd, api()).

lookup(Cmd, Cmds) ->
    lookup(Cmd, 1, Cmds, tuple_size(Cmds)).
lookup(Cmd, N, Cmds, _Max) when Cmd =:= element(N, Cmds) ->
    % Convert to 0 offset
    N-1;
lookup(Cmd, N, Cmds, Max) when N =< Max ->
    lookup(Cmd, N+1, Cmds, Max).
";
static({call,2}) ->
"
call(Port, Command) ->
    call(Port, [], Command, []).
";
static({call,3}) ->
"
call(Port, Command, Options) ->
    call(Port, [], Command, Options).
";
static({call,4}) ->
"
call(Port, Pids, execvp, Arg) when is_port(Port), is_list(Arg) ->
    alcove_drv:cast(Port, encode(execvp, Pids, Arg)),
    ok;
call(Port, Pids, Command, Arg) when is_port(Port), is_list(Arg) ->
    case alcove_drv:call(Port, encode(Command, Pids, Arg)) of
        badarg ->
            erlang:error(badarg, [Port, Command, Arg]);
        Reply ->
            Reply
    end.
".

includes(Header) ->
    [ erl_syntax:attribute(erl_syntax:atom(include), [erl_syntax:string(N)]) || N <- Header ].

% FIXME hack for hard coding typespecs
specs() ->
"
-spec chdir(port(),iodata()) -> 'ok' | {'error', file:posix()}.
-spec chroot(port(),iodata()) -> 'ok' | {'error', file:posix()}.
-spec execvp(port(),iodata(),iodata()) -> 'ok'.
-spec getcwd(port()) -> {'ok', binary()} | {'error', file:posix()}.
-spec getgid(port()) -> non_neg_integer().
-spec gethostname(port()) -> {'ok', binary()} | {'error', file:posix()}.
-spec getpid(port()) -> non_neg_integer().
-spec getrlimit(port(),non_neg_integer()) -> {'ok', #rlimit{}} | {'error', file:posix()}.
-spec getuid(port()) -> non_neg_integer().
-spec ctl(port()) -> 'false' | binary().
-spec ctl(port(),'infinity' | non_neg_integer()) -> 'false' | binary().
-spec setgid(port(),non_neg_integer()) -> 'ok' | {'error', file:posix()}.
-spec sethostname(port(),_) -> 'ok' | {'error', file:posix()}.
-spec setns(port(),iodata()) -> 'ok' | {'error', file:posix()}.
-spec setrlimit(port(),non_neg_integer(),non_neg_integer(),non_neg_integer()) -> 'ok' | {'error', file:posix()}.
-spec setrlimit(port(),non_neg_integer(),#rlimit{}) -> 'ok' | {'error', file:posix()}.
-spec setuid(port(),non_neg_integer()) -> 'ok' | {'error', file:posix()}.
-spec stderr(port(),list(integer())) -> 'false' | binary().
-spec stderr(port(),list(integer()),'infinity' | non_neg_integer()) -> 'false' | binary().
-spec stdin(port(),list(integer()),iodata()) -> 'true'.
-spec stdout(port(),list(integer())) -> 'false' | binary().
-spec stdout(port(),list(integer()),'infinity' | non_neg_integer()) -> 'false' | binary().
-spec version(port()) -> binary().
".
