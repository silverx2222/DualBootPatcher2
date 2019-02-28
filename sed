sed \
    -e "s,@REPLACE_ME_KEYSTORE_PATH@,${TRAVIS_BUILD_DIR}/DualBootPatcher2/silverx2222.jks,g" \
    -e "s,@REPLACE_ME_KEYSTORE_PASSPHRASE@,${keystore_password},g" \
    -e "s,@REPLACE_ME_KEY_ALIAS@,${keystore_alias},g" \
    -e "s,@REPLACE_ME_KEY_PASSPHRASE@,${keystore_alias_password},g" \
    < cmake/SigningConfig.prop.in \
    > cmake/SigningConfig.prop
