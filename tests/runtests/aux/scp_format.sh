date="$(date +%F)"

pushd $( dirname $OUTPUT_ROOT )
tar czvf /tmp/$date.tar.gz $( basename $OUTPUT_ROOT )
popd

export ARCHIVE=/tmp/$date.tar.gz
