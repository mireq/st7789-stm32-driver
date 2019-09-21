target remote localhost:3333
monitor arm semihosting disable
monitor reset halt


#handle SIGTRAP noprint nostop

source semihosting_helper.py
catch signal SIGTRAP
commands
pi on_trap()
end


load
continue
