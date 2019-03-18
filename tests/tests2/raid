#!/bin/sh

# Script to test file on raid or jbod storage device

yes="y"
declare -a devices

testscript="RAID/JBOD SCSI file test script - rev A"
echo $testscript

echo "WOULD YOU LIKE TO INSTALL NAC DRIVER (enter y for yes n for no) THEN ENTER"
read insdriver
if [ $insdriver = $yes ]; then
 ./install
fi

# ****** GATHER INFORMATION FOR TEST ******

echo "WOULD YOU LIKE TO DO FILE TESTING (enter y for yes n for no) THEN ENTER"
read filetest

if [ $filetest = $yes ]; then
 echo "SPECIFY FILE SIZE TO TEST (100 = 100Mbyte) THEN ENTER"
 read filesize
 echo "files size to create is $filesize Mbyte"
 echo "WOULD YOU LIKE TO COPY A FILE FROM HOST TO STORAGE (enter y for yes n for no) THEN ENTER"
 read filecopy
 echo "WOULD YOU LIKE TO DELETE THE FILE FROM STORAGE WHEN TESTING IS COMPLETE (enter y for yes n for no) THEN ENTER"
 read filedelete

# ****** GET STORAGE DEVICES TO WRITE FILE ******

 echo "ENTER STORAGE DEVICE NAMES TO RUN THE FILE TEST ON ONE LINE (ex:sda1 sdb5 sdd6) THEN ENTER"
 read -a devices
 devicecount=${#devices[@]}
 index=0
 while [ "$index" -lt "$devicecount" ]
 do
  echo ${devices[$index]}
  let "index = $index +1"
 done

# **** clear out test1.txt file ****

 cp zero.txt test2.txt

# **** create file of size filesize use testbase.txt to generate
test1.txt

 echo "creating test file... this takes a couple of minutes"
 while [ $filesize -gt 0 ]
  do
    cat
testbase2.txt>>/home/root/DDC/latest/FC7901xS1/ddk/applications/test2.txt
    filesize=$((filesize-1))
  done
 echo "test file test2.txt is created"

# **** copy file to RAID or JBOD?  ********

 if [ $filecopy = $yes ];then
# **** mount storage copy file then sync and compare files
    index=0
    while [ "$index" -lt "$devicecount" ]
    do
     echo "mounting ${devices[$index]}"
     echo "mounting
${devices[$index]}">>/home/root/DDC/latest/FC7901xS1/ddk/applications/filetest.txt
     mount -t ext2 -v /dev/${devices[$index]}
/new_root>>/home/root/DDC/latest/FC7901xS1/ddk/applications/filetest.txt
     echo "copying test file to ${devices[$index]}"
     cp test2.txt /new_root
     echo "sync disks"
     sync
     echo "comparing files now (no output if files are the same)"
     cmp -l test2.txt /new_root/test2.txt
     echo "COMPARE COMPLETE for ${devices[$index]}"
     echo "unmount ${devices[$index]}"
     umount -v /new_root
     let "index = $index + 1"
    done
 fi
 echo "***** FILE TEST COMPLETE *****"

# **** delete file from RAID or JBOD? *******

 if [ $filedelete = $yes ];then
  index=0
  while [ "$index" -lt "$devicecount" ]
  do
   echo "mounting ${devices[$index]}"
   echo "mounting
${devices[$index]}">>/home/root/DDC/latest/FC7901xS1/ddk/applications/filetest.txt
   mount -t ext2 -v /dev/${devices[$index]}
/new_root>>/home/root/DDC/latest/FC7901xS1/ddk/applications/filetest.txt
   echo "deleting file test2.txt from ${devices[$index]}"
   rm /new_root/test2.txt
   echo "test2.txt is now removed from ${devices[$index]}"
   echo "unmount ${devices[$index]}"
   umount -v /new_root
   let "index = $index + 1"
  done
 fi

fi

