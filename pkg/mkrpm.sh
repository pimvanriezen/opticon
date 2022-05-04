#!/bin/bash
version=$(git describe --tags | sed -e 's/-g.*//')
sed -e "s/<<VERSION>>/$version/g" < pkg/opticon-agent.spec.in \
                                  > pkg/opticon-agent.spec
rpmbuild -bp pkg/opticon-agent.spec
rm -f pkg/opticon-agent.spec
