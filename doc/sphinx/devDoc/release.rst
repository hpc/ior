Release Process
===============

General release process
-----------------------

The versioning for IOR is encoded in the ``META`` file in the root of the
repository.  The nomenclature is

* 3.2.0 designates a proper release
* 3.2.0rc1 designates the first release candidate in preparation for the 3.2.0
  release
* 3.2.0+dev indicates development towards 3.2.0 prior to a feature freeze
* 3.2.0rc1+dev indicates development towards 3.2.0's first release candidate
  after a feature freeze

Building a release of IOR
-------------------------

To build a new version of IOR::

    $ docker run -it ubuntu bash
    $ apt-get update
    $ apt-get install -y git automake autoconf make gcc mpich
    $ git clone -b rc https://github.com/hpc/ior
    $ cd ior
    $ ./travis-build.sh

Feature freezing for a new release
----------------------------------

1. Branch `major.minor` from the commit at which the feature freeze should take
   effect.
2. Append the "rc1+dev" designator to the Version field in the META file
3. Commit and push this new branch
2. Update the ``Version:`` field in META `of the master branch` to be the `next`
   release version, not the one whose features have just been frozen.

For example, to feature-freeze for version 3.2::

    $ git checkout 11469ac
    $ git checkout -B 3.2
    $ # update the ``Version:`` field in ``META`` to 3.2.0rc1+dev
    $ git add META
    $ git commit -m "Update version for feature freeze"
    $ git push upstream 3.2
    $ git checkout master
    $ # update the ``Version:`` field in ``META`` to 3.3.0+dev
    $ git add META
    $ git commit -m "Update version number"
    $ git push upstream master

Creating a new release candidate
--------------------------------

1. Check out the appropriate commit from the `major.minor` branch
2. Disable the ``check-news`` option in ``AM_INIT_AUTOMAKE`` inside configure.ac
3. Remove the "+dev" designator from the Version field in META
4. Build a release package as described above
5. Revert the change from #2 (it was just required to build a non-release tarball)
5. Tag and commit the updated META so one can easily recompile this rc from git
6. Update the "rcX" number and add "+dev" back to the ``Version:`` field in
   META.  This will allow anyone playing with the tip of this branch to see that
   this the state is in preparation of the next rc, but is unreleased because of
   +dev.
7. Commit

For example to release 3.2.0rc1::

    $ git checkout 3.2
    $ # edit configure.ac and remove the check-news option
    $ # remove +dev from the Version field in META (Version: 3.2.0rc1)
    $ # build
    $ git checkout configure.ac
    $ git add META
    $ git commit -m "Release candidate for 3.2.0rc1"
    $ git tag 3.2.0rc1
    $ # uptick rc number and re-add +dev to META (Version: 3.2.0rc2+dev)
    $ git add META # should contain Version: 3.2.0rc2+dev
    $ git commit -m "Uptick version after release"
    $ git push --tags

Applying patches to a new microrelease
--------------------------------------

If a released version 3.2.0 has bugs, cherry-pick the fixes from master into the
3.2 branch::

    $ git checkout 3.2
    $ git cherry-pick cb40c99
    $ git cherry-pick aafdf89
    $ git push upstream 3.2

Once you've accumulated enough bugs, move on to issuing a new release below.

Creating a new release
----------------------

1. Check out the relevant `major.minor` branch
2. Remove any "rcX" and "+dev" from the Version field in META
3. Update NEWS with the release notes
4. Build a release package as described above
5. Tag and commit the updated NEWS and META so one can easily recompile this
   release from git
6. Update the Version field to the next rc version and re-add "+dev"
7. Commit
8. Create the major.minor.micro release on GitHub from the associated tag

For example to release 3.2.0::

    $ git checkout 3.2
    $ vim META # 3.2.0rc2+dev -> 3.2.0
    $ vim NEWS # add release notes from ``git log --oneline 3.2.0rc1..``
    $ # build
    $ git add NEWS META
    $ git commit -m "Release v3.2.0"
    $ git tag 3.2.0
    $ vim META # 3.2.0 -> 3.2.1rc1+dev
    $ git add META
    $ git commit -m "Uptick version after release"
    $ git push --tags
