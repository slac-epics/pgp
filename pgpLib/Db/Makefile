TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

#----------------------------------------------------
#  Optimization of db files using dbst (DEFAULT: NO)
#DB_OPT = YES

#----------------------------------------------------
# Create and install (or just install)
# databases, templates, substitutions like this
# DB += pgp.db
DB += pgp_common.db
DB += pgp_reg.db
DB += pgp_reg_rbonly.db

#----------------------------------------------------
# Create and install (or just install)
# Autosave request files, templates, substitutions like this
# REQ += autosave_pgp.req
REQ += autosave_pgp_reg.req

#----------------------------------------------------
# Create and install (or just install)
# Archive files, templates, substitutions like this
# ARCHIVE += archive_pgp.archive

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

