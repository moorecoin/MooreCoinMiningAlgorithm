aclocal_amflags = -i build-aux/m4
subdirs = src
.phony: deploy force

gzip_env="-9n"

if build_bitcoin_libs
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_data = libbitcoinconsensus.pc
endif

bitcoind_bin=$(top_builddir)/src/bitcoind$(exeext)
bitcoin_qt_bin=$(top_builddir)/src/qt/bitcoin-qt$(exeext)
bitcoin_cli_bin=$(top_builddir)/src/bitcoin-cli$(exeext)
bitcoin_win_installer=$(package)-$(package_version)-win$(windows_bits)-setup$(exeext)

osx_app=bitcoin-qt.app
osx_dmg=bitcoin-core.dmg
osx_background_image=background.tiff
osx_deploy_script=$(top_srcdir)/contrib/macdeploy/macdeployqtplus
osx_fancy_plist=$(top_srcdir)/contrib/macdeploy/fancy.plist
osx_base_lproj_dir=$(top_srcdir)/contrib/macdeploy/base.lproj/infoplist.strings
osx_installer_icons=$(top_srcdir)/src/qt/res/icons/bitcoin.icns
osx_plist=$(top_srcdir)/share/qt/info.plist #not installed
osx_qt_translations = da,de,es,hu,ru,uk,zh_cn,zh_tw

dist_docs = $(wildcard doc/*.md) $(wildcard doc/release-notes/*.md)

windows_packaging = $(top_srcdir)/share/pixmaps/bitcoin.ico \
  $(top_srcdir)/share/pixmaps/nsis-header.bmp \
  $(top_srcdir)/share/pixmaps/nsis-wizard.bmp \
  $(top_srcdir)/doc/readme_windows.txt

osx_packaging = $(osx_deploy_script) $(osx_fancy_plist) $(osx_installer_icons) $(osx_base_lproj_dir) \
  $(top_srcdir)/contrib/macdeploy/$(osx_background_image) \
  $(top_srcdir)/contrib/macdeploy/ds_store \
  $(top_srcdir)/contrib/macdeploy/detached-sig-apply.sh \
  $(top_srcdir)/contrib/macdeploy/detached-sig-create.sh

coverage_info = baseline_filtered_combined.info baseline.info block_test.info \
  leveldb_baseline.info test_bitcoin_filtered.info total_coverage.info \
  baseline_filtered.info block_test_filtered.info \
  leveldb_baseline_filtered.info test_bitcoin_coverage.info test_bitcoin.info

dist-hook:
	-$(make) -c $(top_distdir)/src/leveldb clean
	-$(make) -c $(top_distdir)/src/secp256k1 distclean
	-$(git) archive --format=tar head -- src/clientversion.cpp | $(amtar) -c $(top_distdir) -xf -

distcheck-hook:
	$(mkdir_p) $(top_distdir)/_build/src/leveldb
	cp -rf $(top_srcdir)/src/leveldb/* $(top_distdir)/_build/src/leveldb/
	-$(make) -c $(top_distdir)/_build/src/leveldb clean

distcleancheck:
	@:

$(bitcoin_win_installer): all-recursive
	$(mkdir_p) $(top_builddir)/release
	stripprog="$(strip)" $(install_strip_program) $(bitcoind_bin) $(top_builddir)/release
	stripprog="$(strip)" $(install_strip_program) $(bitcoin_qt_bin) $(top_builddir)/release
	stripprog="$(strip)" $(install_strip_program) $(bitcoin_cli_bin) $(top_builddir)/release
	@test -f $(makensis) && $(makensis) -v2 $(top_builddir)/share/setup.nsi || \
	  echo error: could not build $@
	@echo built $@

$(if $(findstring src/,$(makecmdgoals)),$(makecmdgoals), none): force
	$(make) -c src $(patsubst src/%,%,$@)

$(osx_app)/contents/pkginfo:
	$(mkdir_p) $(@d)
	@echo "appl????" > $@

$(osx_app)/contents/resources/empty.lproj:
	$(mkdir_p) $(@d)
	@touch $@ 

$(osx_app)/contents/info.plist: $(osx_plist)
	$(mkdir_p) $(@d)
	$(install_data) $< $@

$(osx_app)/contents/resources/bitcoin.icns: $(osx_installer_icons)
	$(mkdir_p) $(@d)
	$(install_data) $< $@

$(osx_app)/contents/macos/bitcoin-qt: $(bitcoin_qt_bin)
	$(mkdir_p) $(@d)
	stripprog="$(strip)" $(install_strip_program)  $< $@

$(osx_app)/contents/resources/base.lproj/infoplist.strings: $(osx_base_lproj_dir)
	$(mkdir_p) $(@d)
	$(install_data) $< $@

osx_app_built=$(osx_app)/contents/pkginfo $(osx_app)/contents/resources/empty.lproj \
  $(osx_app)/contents/resources/bitcoin.icns $(osx_app)/contents/info.plist \
  $(osx_app)/contents/macos/bitcoin-qt $(osx_app)/contents/resources/base.lproj/infoplist.strings

if build_darwin
$(osx_dmg): $(osx_app_built) $(osx_packaging)
	$(osx_deploy_script) $(osx_app) -add-qt-tr $(osx_qt_translations) -translations-dir=$(qt_translation_dir) -dmg -fancy $(osx_fancy_plist) -verbose 2

deploydir: $(osx_dmg)
else
app_dist_dir=$(top_builddir)/dist
app_dist_extras=$(app_dist_dir)/.background/$(osx_background_image) $(app_dist_dir)/.ds_store $(app_dist_dir)/applications

$(app_dist_dir)/applications:
	@rm -f $@
	@cd $(@d); $(ln_s) /applications $(@f)

$(app_dist_extras): $(app_dist_dir)/$(osx_app)/contents/macos/bitcoin-qt

$(osx_dmg): $(app_dist_extras)
	$(genisoimage) -no-cache-inodes -d -l -probe -v "bitcoin-core" -no-pad -r -apple -o $@ dist

$(app_dist_dir)/.background/$(osx_background_image): contrib/macdeploy/$(osx_background_image)
	$(mkdir_p) $(@d)
	$(install) $< $@
$(app_dist_dir)/.ds_store: contrib/macdeploy/ds_store
	$(install) $< $@

$(app_dist_dir)/$(osx_app)/contents/macos/bitcoin-qt: $(osx_app_built) $(osx_packaging)
	installnametool=$(installnametool)  otool=$(otool) strip=$(strip) $(osx_deploy_script) $(osx_app) -translations-dir=$(qt_translation_dir) -add-qt-tr $(osx_qt_translations) -verbose 2

deploydir: $(app_dist_extras)
endif

if target_darwin
appbundle: $(osx_app_built)
deploy: $(osx_dmg)
endif
if target_windows
deploy: $(bitcoin_win_installer)
endif

$(bitcoin_qt_bin): force
	$(make) -c src qt/$(@f)

$(bitcoind_bin): force
	$(make) -c src $(@f)

$(bitcoin_cli_bin): force
	$(make) -c src $(@f)

if use_lcov

baseline.info:
	$(lcov) -c -i -d $(abs_builddir)/src -o $@

baseline_filtered.info: baseline.info
	$(lcov) -r $< "/usr/include/*" -o $@

leveldb_baseline.info: baseline_filtered.info
	$(lcov) -c -i -d $(abs_builddir)/src/leveldb -b $(abs_builddir)/src/leveldb -o $@

leveldb_baseline_filtered.info: leveldb_baseline.info
	$(lcov) -r $< "/usr/include/*" -o $@

baseline_filtered_combined.info: leveldb_baseline_filtered.info baseline_filtered.info
	$(lcov) -a leveldb_baseline_filtered.info -a baseline_filtered.info -o $@

test_bitcoin.info: baseline_filtered_combined.info
	$(make) -c src/ check
	$(lcov) -c -d $(abs_builddir)/src -t test_bitcoin -o $@
	$(lcov) -z -d $(abs_builddir)/src
	$(lcov) -z -d $(abs_builddir)/src/leveldb

test_bitcoin_filtered.info: test_bitcoin.info
	$(lcov) -r $< "/usr/include/*" -o $@

block_test.info: test_bitcoin_filtered.info
	$(mkdir_p) qa/tmp
	-@timeout=15 qa/pull-tester/run-bitcoind-for-test.sh $(java) -jar $(java_comparison_tool) qa/tmp/comptool 0
	$(lcov) -c -d $(abs_builddir)/src --t bitcoinjblocktest -o $@
	$(lcov) -z -d $(abs_builddir)/src
	$(lcov) -z -d $(abs_builddir)/src/leveldb

block_test_filtered.info: block_test.info
	$(lcov) -r $< "/usr/include/*" -o $@

test_bitcoin_coverage.info: baseline_filtered_combined.info test_bitcoin_filtered.info
	$(lcov) -a baseline_filtered.info -a leveldb_baseline_filtered.info -a test_bitcoin_filtered.info -o $@

total_coverage.info:  baseline_filtered_combined.info test_bitcoin_filtered.info block_test_filtered.info
	$(lcov) -a baseline_filtered.info -a leveldb_baseline_filtered.info -a test_bitcoin_filtered.info -a block_test_filtered.info -o $@ | $(grep) "\%" | $(awk) '{ print substr($$3,2,50) "/" $$5 }' > coverage_percent.txt

test_bitcoin.coverage/.dirstamp:  test_bitcoin_coverage.info
	$(genhtml) -s $< -o $(@d)
	@touch $@

total.coverage/.dirstamp: total_coverage.info
	$(genhtml) -s $< -o $(@d)
	@touch $@

cov: test_bitcoin.coverage/.dirstamp total.coverage/.dirstamp

endif

if use_comparison_tool
check-local:
	$(mkdir_p) qa/tmp
	@qa/pull-tester/run-bitcoind-for-test.sh $(java) -jar $(java_comparison_tool) qa/tmp/comptool $(comparison_tool_reorg_tests) 2>&1
endif

extra_dist = $(top_srcdir)/share/genbuild.sh qa/pull-tester/rpc-tests.sh qa/pull-tester/run-bitcoin-cli qa/rpc-tests $(dist_docs) $(windows_packaging) $(osx_packaging)

cleanfiles = $(osx_dmg) $(bitcoin_win_installer)

.intermediate: $(coverage_info)

clean-local:
	rm -rf test_bitcoin.coverage/ total.coverage/ $(osx_app)
