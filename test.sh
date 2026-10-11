#!/bin/bash
set -e

IMAGE="ext2_test.img"
MNT="mnt"

truncate -s 500M $IMAGE
mkfs.ext2 -b 2048 -N 2000 -I 128 $IMAGE

mkdir -p $MNT
sudo mount -t ext2 $IMAGE $MNT
sudo chown $USER:$USER $MNT

echo "small" > $MNT/small.txt
dd if=/dev/urandom of=$MNT/medium.bin bs=1024 count=200 2>/dev/null
truncate -s 5G $MNT/sparse.bin
echo "sparse" > $MNT/sparse.bin
dd if=/dev/urandom bs=4096 count=10 of=$MNT/sparse.bin seek=1000 conv=notrunc 2>/dev/null
mkdir -p $MNT/dir1/subdir $MNT/dir2
echo "file1" > $MNT/dir1/file1.txt
echo "file2" > $MNT/dir1/subdir/file2.txt
echo "file3" > $MNT/dir2/file3.txt

inode_small=$(ls -i $MNT/small.txt | awk '{print $1}')
inode_medium=$(ls -i $MNT/medium.bin | awk '{print $1}')
inode_sparse=$(ls -i $MNT/sparse.bin | awk '{print $1}')
inode_dir1=$(ls -di $MNT/dir1 | awk '{print $1}')
inode_dir2=$(ls -di $MNT/dir2 | awk '{print $1}')
inode_subdir=$(ls -di $MNT/dir1/subdir | awk '{print $1}')

sha_small=$(sha512sum $MNT/small.txt | awk '{print $1}')
sha_medium=$(sha512sum $MNT/medium.bin | awk '{print $1}')
sha_sparse=$(sha512sum $MNT/sparse.bin | awk '{print $1}')

ls -ai $MNT/dir1 | awk '{print $1, $2}' | sort > dir1_expected.txt
ls -ai $MNT/dir2 | awk '{print $1, $2}' | sort > dir2_expected.txt
ls -ai $MNT/dir1/subdir | awk '{print $1, $2}' | sort > subdir_expected.txt

sudo umount $MNT

./util1 $IMAGE $inode_small > /dev/null || { echo "fail: util1 small.txt"; exit 1; }
./util1 $IMAGE $inode_medium > /dev/null || { echo "fail: util1 medium.bin"; exit 1; }
./util1 $IMAGE $inode_sparse > /dev/null || { echo "fail: util1 sparse.bin"; exit 1; }

./util2 $IMAGE $inode_small > small.txt.extracted || { echo "fail: util2 small.txt"; exit 1; }
./util2 $IMAGE $inode_medium > medium.bin.extracted || { echo "fail: util2 medium.bin"; exit 1; }
./util2 $IMAGE $inode_sparse > sparse.bin.extracted || { echo "fail: util2 sparse.bin"; exit 1; }

[ "$sha_small" = "$(sha512sum small.txt.extracted | awk '{print $1}')" ] || { echo "fail: small.txt checksum"; exit 1; }
[ "$sha_medium" = "$(sha512sum medium.bin.extracted | awk '{print $1}')" ] || { echo "fail: medium.bin checksum"; exit 1; }
[ "$sha_sparse" = "$(sha512sum sparse.bin.extracted | awk '{print $1}')" ] || { echo "fail: sparse.bin checksum"; exit 1; }

./util2 $IMAGE $inode_dir1 | ./util3 | grep -vE "Inode|-----" | awk '{print $1, $3}' | sort > dir1_util3.txt
diff dir1_expected.txt dir1_util3.txt > /dev/null || { echo "fail: dir1 listing"; exit 1; }

./util2 $IMAGE $inode_dir2 | ./util3 | grep -vE "Inode|-----" | awk '{print $1, $3}' | sort > dir2_util3.txt
diff dir2_expected.txt dir2_util3.txt > /dev/null || { echo "fail: dir2 listing"; exit 1; }

./util2 $IMAGE $inode_subdir | ./util3 | grep -vE "Inode|-----" | awk '{print $1, $3}' | sort > subdir_util3.txt
diff subdir_expected.txt subdir_util3.txt > /dev/null || { echo "fail: subdir listing"; exit 1; }

loop=$(sudo losetup -f)
sudo losetup $loop $IMAGE
sudo chmod 644 $loop
./util1 $loop $inode_small > /dev/null || { echo "fail: loop util1"; exit 1; }
./util2 $loop $inode_small | sha512sum | grep -q "$sha_small" || { echo "fail: loop util2"; exit 1; }
lsblk -o name,size,fstype
sudo losetup -d $loop

echo "all tests passed"