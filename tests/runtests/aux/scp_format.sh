date="$(date +%F)"

tar czvf $date.tar.gz $OUTPUT_ROOT
export ARCHIVE=$date.tar.gz
