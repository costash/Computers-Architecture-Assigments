#!/bin/bash

CONVERT="" #conversion tool
STREAMS="streams/" #streams folder
IMAGES="images/" #images folder

if command -v avconv >/dev/null 2>&1; then
	CONVERT="avconv"
elif command -v "ffmpeg" >/dev/null 2>&1; then
	CONVERT="ffmpeg"
else
	echo "Neither avconv nor ffmpeg is installed. Please install one of them."
	exit
fi

echo "Using $CONVERT for conversion."

if [ -d $STREAMS ]; then
	mkdir -p $IMAGES
	for filename in `ls $STREAMS`; do
		name=${filename:0:(-4)} #cut the extension
		mkdir -p $IMAGES$name
		$CONVERT -i $STREAMS$filename $IMAGES$name/image%d.pnm
	done
else
	echo "Streams folder does not exist."
	exit
fi

echo "Done"
