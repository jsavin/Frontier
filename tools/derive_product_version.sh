#!/bin/sh
# Derive product version from CLI version tag.
# CLI major + 10, stage mapped: alpha→a, beta→b, rc→fc, dev→d
# Examples:
#   v1.0.0-alpha.6       → 11.0a6
#   v1.0.0-beta.3        → 11.0b3
#   v1.0.0               → 11.0
#   v1.1.0-alpha.2       → 11.1a2
#   v1.2.3               → 11.2.3
#   1.0.0-dev             → 11.0d

v="${1#v}"  # strip leading 'v' if present

major=$(echo "$v" | sed -E 's/^([0-9]+).*/\1/')
rest=$(echo "$v" | sed -E 's/^[0-9]+\.//')
pmajor=$((major + 10))
minor=$(echo "$rest" | sed -E 's/^([0-9]+).*/\1/')
rest2=$(echo "$rest" | sed -E 's/^[0-9]+\.//')
patch=$(echo "$rest2" | sed -E 's/^([0-9]+).*/\1/')

stage=""
sub=""
if echo "$v" | grep -qE '\-(alpha|beta|rc|dev)'; then
    stage=$(echo "$v" | sed -E 's/.*-(alpha|beta|rc|dev).*/\1/')
    sub=$(echo "$v" | sed -E 's/.*-(alpha|beta|rc|dev)\.?//; s/[^0-9].*//')
    case "$stage" in
        alpha) stage="a" ;;
        beta)  stage="b" ;;
        rc)    stage="fc" ;;
        dev)   stage="d" ;;
    esac
fi

if [ "$patch" = "0" ] || [ "$patch" = "$minor" ]; then
    echo "${pmajor}.${minor}${stage}${sub}"
else
    echo "${pmajor}.${minor}.${patch}${stage}${sub}"
fi
