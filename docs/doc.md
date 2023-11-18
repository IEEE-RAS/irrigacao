## Trava inicial

@startuml
!pragma useVerticalSwitch on
start
:receive package;
switch(packet[0])
case (S)
    :Select timing;
    :Update State;
case (C)
    :Movement with timer;
    :update State;
case (U)
    :Movement no timer;
    :update State;
case (B)
    :Stop;
    :update State;
case (?)
    :Help;
    :update State;
case ($)
    :Unlock;
    :update State;
endswitch
stop

@enduml

@startuml
    state T as "Travado" : Mostrar mesnagem de alerta
    state D as "Destravado" : Executa loop principal
    state H as "Help" : Envia mensagem de ajuda

    [*]-d->T  : xxx
    T->T    : xxx
    T-->H   : ?
    H-->T
    T-l->D  : $
    D-d->[*]   
@enduml

@startuml
    hide empty description

    state "Novo Pacote" as N
    state "Byte 1" as B1 : Seletor de funções
    state "Byte 2" as B2 : Parametro funções
    state "Default" as DF 

    [*] --> N
    N -d->B1 : Chegou
    N --> N : Não chegou

    B1 -d-> B2 : Byte Valido
    B2 -d-> DF : Byte Invalido
    B1 -l-> DF : Byte Invalido

    DF -u-> N

@enduml

@startuml
start

:set direction flag
set state to move;

:start movement;
while(not endswitch)
    :read endswitch;
endwhile
:set state to end;
:stop;
end
@enduml

@startuml
start
if(END?) then (true)
    if(SAMEDIRECTION) then (true)
    else (false)
        :moveUntil;
    endif
else (false)
    :moveUntil;
endif
:stop;
end
@enduml

@startuml
start
:verifyPacket;
:parsePacket;
switch(packet[0])
case (S)
    :Select timing;
    :Update State;
case (C)
    :Movement with timer;
    :update State;
case (U)
    :Movement no timer;
    :update State;
case (B)
    :Stop;
    :update State;
case (E)
    :Movement til endstop;
    :update State;
case (?)
    :Help;
    :update State;
case ($)
    :Unlock;
    :update State;
endswitch
:runState;
:verifyLimitSensor
:verifyTime;
if (Limit?) then (yes)
    :stop();
    :updateEndState;
endif
if (Timeout?) then (yes)
    :stop();
    :updateTimeState;
endif
@enduml