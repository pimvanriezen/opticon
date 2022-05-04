#!/bin/bash
versionstr=$(git describe --tags | sed -e 's/-g.*//')
version=$(echo "$versionstr" | cut -f1 -d-)
release=$(echo "$versionstr" | cut -f2 -d-)

sed -e "s/<<VERSION>>/$version/g;s/<<RELEASE>>/$release/g" \
    < pkg/opticon-agent.spec.in > pkg/opticon-agent.spec
rpmbuild --noprep pkg/opticon-agent.spec
rm -f pkg/opticon-agent.spec
