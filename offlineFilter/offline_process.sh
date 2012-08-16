cp RECORD_THREAD_MAP.log THREAD_MAP.log
cp RECORD_THREAD_MAP.log REPLAY_THREAD_MAP.log

./interFilter.out
rm *.tmp

./allocFilter.out
rm *.tmp

