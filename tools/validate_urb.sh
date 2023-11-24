#!/bin/bash
for file in $(ls ../tmp/*.output); do
	sort $file > ${file}_sorted
done
for file1 in $(ls ../tmp/*_sorted); do
	for file2 in $(ls ../tmp/*_sorted); do
		diff $file1 $file2 -q
	done
done
