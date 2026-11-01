set pagination off
set confirm off
set width 0
set print elements 200
set breakpoint pending on
break main
run
info sharedlibrary
delete breakpoints
break free
commands
silent
printf "FREE_BREAKPOINT_COVERAGE 1\n"
delete 2
continue
end
break free if $rcx > 0x10000 && *(unsigned long long*)($rcx - 16) == $rcx - 16
set $hits = 0
commands
silent
set $hits = $hits + 1
set $ret = *(void**)$rsp
printf "PAYLOAD_FREE %d ptr=%p wm16=0x%016llx wm8=0x%016llx probe_hit=%d ret=%p\n", $hits, $rcx, *(unsigned long long*)($rcx - 16), *(unsigned long long*)($rcx - 8), $rcx == (unsigned long long)cux_probe_buffer, $ret
printf "CALL_SITE %s\n", "return address belongs to:"
info symbol $ret
printf "CALL_SITE_CODE\n"
x/5i ((char*)$ret) - 12
printf "BACKTRACE\n"
bt 4
if $hits >= 2
  printf "KILLING_AFTER_SECOND_HIT\n"
  kill
end
continue
end
continue
printf "PAYLOAD_FREE_HITS %d\n", $hits
