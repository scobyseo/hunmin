#!/bin/bash
# SPDX-License-Identifier: LGPL-2.1-or-later
#
# Fetch the two AIHub corpora this project's MODES measurements depend on.
# Neither is vendored: AIHub's terms are use-and-training only, redistribution
# is not permitted, and the outputs (235 MB + 155 MB) belong outside the repo
# anyway. Only the converted tables are the instrument, and even those may not
# be shipped in the deb -- see docs/MODES-DATA.md 2.
#
# THE ROUTE IS aihubshell, a standalone script from the same operator as the
# API. The dataset page needs a login session AND a captcha; the MCP server
# exposes metadata only; the catalogue API answers X-API-KEY against /mcp/*.do
# with "잘못된 접근입니다." which looks like bad auth and is not. So:
#
#   curl -o aihubshell https://api.aihub.or.kr/api/aihubshell.do && chmod +x aihubshell
#   ./aihubshell -mode l | grep 말뭉치                        # list  -> datasetkey
#   ./aihubshell -mode l -datasetkey 624                     # files -> filekey
#   ./aihubshell -mode d -datasetkey 624 -filekey 32786 -aihubapikey 'KEY'
#
# Quote the key in single quotes; it contains characters the shell treats
# specially. -filekey accepts a comma list, so fetch only the source-text
# member (TS1.zip) rather than the labels.
#
# TWO KEY NAMESPACES, do not mix them up:
#
#   -mode l  -datasetkey 624          dataset  -> files -> filekey
#   -mode pl                             data PACKAGE -> files -> filekey
#   -mode pl  -datapckagekey N          ...and the file's own datapckagekey
#
# A datapckagekey is NOT a datasetkey and vice versa. AIHub's own help says
# "datapckagekey ... -mode pl 에서 datapckagekey 번호를 확인 할 수 있습니다",
# i.e. a package key is only discoverable through pl, and a datasetkey only
# through l. Feeding one to the other silently finds nothing. This project's
# two corpora are datasets, so the route below is the -datasetkey one; the
# package path exists because it is the other half of the tool and a reader
# holding only a datapckagekey has no way to tell which flag to reach for.
#
#   ./aihubshell -mode pl | grep 말뭉치                     # package list
#   ./aihubshell -mode pl -datapckagekey 12345             # its files
#   ./aihubshell -mode pd -datapckagekey 12345 -filekey 678 -aihubapikey 'KEY'
#
# PRECONDITION: the download must already be approved. AIHub's help states it as
# a requirement, twice, in the words "[필수 사전 사항] https://aihub.or.kr 에서
# 데이터 승인신청 및 승인완료" -- apply on the website and wait for approval to
# complete, THEN run this. A download before that does not fail with a useful
# message; it just does not arrive. This is a workflow fact, not a CLI fact, so
# it is invisible to anyone reading --help. It has been obtained once already
# (requests 720837 / 720834) and a re-run by anyone else needs their own.
#
# DISK: allow 2-3x the size of the data being downloaded. Not because of the
# data: the tool merges the archives, unpacks them into the directory structure,
# and deletes the zip, so the peak holds the archive and its expansion at once.
# AIHub's help says "다운로드 된 파일을 압축해제하기 위해 다운로드 받는 데이터의
# 2~3배 이상의 용량을 확보하시길 바랍니다". The header above used to argue size
# from licence and output alone, which is a different question and the wrong one.
#
# usage: tools/fetch-aihub.sh [OUTDIR]        default OUTDIR=data-orig/aihub
set -eu
OUT=${1:-"$(dirname "$0")/../data-orig/aihub"}
mkdir -p "$OUT"

want() {   # want <datasetkey> <expected member> <bytes lower bound>
  local key=$1 member=$2 min=$3
  local dest="$OUT/$key.zip"
  if [ -s "$dest" ] && [ "$(stat -c%s "$dest")" -ge "$min" ]; then
    echo "fetch-aihub: $key.zip present ($(stat -c%s "$dest") bytes)"
  else
    echo "fetch-aihub: need $key ($member). Get a filekey with:" >&2
    echo "  ./aihubshell -mode l -datasetkey $key" >&2
    echo "then:" >&2
    echo "  ./aihubshell -mode d -datasetkey $key -filekey <FILEKEY> -aihubapikey 'KEY'" >&2
    return 1
  fi
  # Verify the archive rather than trusting the byte count: a partial download
  # otherwise produces a plausible, quietly truncated table.
  unzip -tq "$dest" >/dev/null || { echo "fetch-aihub: $key.zip is not a valid archive" >&2; return 1; }
  unzip -l "$dest" | grep -q "$member" || {
    echo "fetch-aihub: $key.zip has no $member" >&2; return 1; }
}

# 624 = AIHub 030, 웹데이터 기반 한국어 말뭉치. The only source with typed
# english in it, and therefore the closest proxy for this feature's population.
want 624 'TS1.zip' 100000000 || true
# 119 = 국가기록물. Official administrative prose -- a register neither wikipedia
# nor 624 has, and the source of the 5.90% latin-token markup contamination.
want 119 '말뭉치' 100000000 || true

cat <<EOF

fetch-aihub: archives in $OUT
  then build:
    tools/build-modes-624.py --zip $OUT/624.zip --out ~/work/aihub-records-corpus
    tools/build-modes-624.py --compare ~/work/aihub-records-corpus/624-ko.txt
    tools/build-modes-119.py --zip <the 119 archive> --out ~/work/wikicorpus/119-clean

  ko.wikipedia is separate and has its own script:
    tools/fetch-wikipedia-dump.sh
    tools/build-modes-corpus.py --dump <dump> --out ~/work/wikicorpus/full
EOF
