#!/bin/bash
versionstr=$(git describe --tags | sed -e 's/-g.*//')
version=$(echo "$versionstr" | cut -f1 -d-)
release=$(echo "$versionstr" | cut -f2 -d-)

mkdir -p pkg/tmp
git archive --format=tar.gz -o ~/rpmbuild/SOURCES/opticon-${versionstr}.tar.gz --prefix=opticon-${versionstr}/ master
sed -e "s/<<VERSION>>/$version/g;s/<<RELEASE>>/$release/g" \
    < pkg/opticon-agent.spec.in > pkg/tmp/opticon-agent.spec
rpmbuild -ba pkg/tmp/opticon-agent.spec
