rem make sure we have a safe environement
set LIBRARY=
set INCLUDE=

mkdir vm
cd vm
set cc=lcc -DQ3_VM -S -Wf-target=bytecode -Wf-g -I..\..\cgame -I..\..\game -I..\..\ui %1

%cc%  ../g_main.c
@if errorlevel 1 goto quit

%cc%  ../g_syscalls.c
@if errorlevel 1 goto quit

%cc%  ../bg_misc.c
@if errorlevel 1 goto quit
%cc%  ../bg_lib.c
@if errorlevel 1 goto quit
%cc%  ../bg_pmove.c
@if errorlevel 1 goto quit
%cc%  ../bg_slidemove.c
@if errorlevel 1 goto quit
%cc%  ../q_math.c
@if errorlevel 1 goto quit
%cc%  ../q_shared.c
@if errorlevel 1 goto quit

%cc%  ../g_active.c
@if errorlevel 1 goto quit
%cc%  ../g_arenas.c
@if errorlevel 1 goto quit
%cc%  ../g_bot.c
@if errorlevel 1 goto quit
%cc%  ../g_client.c
@if errorlevel 1 goto quit
%cc%  ../g_cmds.c
@if errorlevel 1 goto quit
%cc%  ../g_combat.c
@if errorlevel 1 goto quit
%cc%  ../g_items.c
@if errorlevel 1 goto quit
%cc%  ../g_mem.c
@if errorlevel 1 goto quit
%cc%  ../g_misc.c
@if errorlevel 1 goto quit
%cc%  ../g_missile.c
@if errorlevel 1 goto quit
%cc%  ../g_mover.c
@if errorlevel 1 goto quit
%cc%  ../g_session.c
@if errorlevel 1 goto quit
%cc%  ../g_spawn.c
@if errorlevel 1 goto quit
%cc%  ../g_svcmds.c
@if errorlevel 1 goto quit
%cc%  ../g_target.c
@if errorlevel 1 goto quit
%cc%  ../g_team.c
@if errorlevel 1 goto quit
%cc%  ../g_trigger.c
@if errorlevel 1 goto quit
%cc%  ../g_utils.c
@if errorlevel 1 goto quit
%cc%  ../g_weapon.c
@if errorlevel 1 goto quit

:ai
%cc%  ../ai_accuracy.c
@if errorlevel 1 goto quit
%cc%  ../ai_action.c
@if errorlevel 1 goto quit
%cc%  ../ai_aim.c
@if errorlevel 1 goto quit
%cc%  ../ai_attack.c
@if errorlevel 1 goto quit
%cc%  ../ai_aware.c
@if errorlevel 1 goto quit
%cc%  ../ai_chat.c
@if errorlevel 1 goto quit
%cc%  ../ai_client.c
@if errorlevel 1 goto quit
%cc%  ../ai_command.c
@if errorlevel 1 goto quit
%cc%  ../ai_dodge.c
@if errorlevel 1 goto quit
%cc%  ../ai_entity.c
@if errorlevel 1 goto quit
%cc%  ../ai_goal.c
@if errorlevel 1 goto quit
%cc%  ../ai_item.c
@if errorlevel 1 goto quit
%cc%  ../ai_level.c
@if errorlevel 1 goto quit
%cc%  ../ai_lib.c
@if errorlevel 1 goto quit
%cc%  ../ai_main.c
@if errorlevel 1 goto quit
%cc%  ../ai_maingoal.c
@if errorlevel 1 goto quit
%cc%  ../ai_motion.c
@if errorlevel 1 goto quit
%cc%  ../ai_move.c
@if errorlevel 1 goto quit
%cc%  ../ai_order.c
@if errorlevel 1 goto quit
%cc%  ../ai_path.c
@if errorlevel 1 goto quit
%cc%  ../ai_pickup.c
@if errorlevel 1 goto quit
%cc%  ../ai_predict.c
@if errorlevel 1 goto quit
%cc%  ../ai_region.c
@if errorlevel 1 goto quit
%cc%  ../ai_resource.c
@if errorlevel 1 goto quit
%cc%  ../ai_scan.c
@if errorlevel 1 goto quit
%cc%  ../ai_self.c
@if errorlevel 1 goto quit
%cc%  ../ai_subteam.c
@if errorlevel 1 goto quit
%cc%  ../ai_team.c
@if errorlevel 1 goto quit
%cc%  ../ai_use.c
@if errorlevel 1 goto quit
%cc%  ../ai_vars.c
@if errorlevel 1 goto quit
%cc%  ../ai_view.c
@if errorlevel 1 goto quit
%cc%  ../ai_visible.c
@if errorlevel 1 goto quit
%cc%  ../ai_waypoint.c
@if errorlevel 1 goto quit
%cc%  ../ai_weapon.c
@if errorlevel 1 goto quit

q3asm -f ../game
del \quake3\brainworks\vm\qagame.map
:quit
cd ..
