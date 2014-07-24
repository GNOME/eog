@screenshot
Feature: Screenshot tour

    Scenario Outline: Main dialogs
     * Set locale to "<locale>"
     * Make sure that eog is running
     * Select and close "Image" menu
     * Select and close "Edit" menu
     * Select and close "View" menu
     * Select and close "Go" menu
     * Select and close "Help" menu
     * Open "/tmp/gnome-logo.png" via menu
     * Open context menu for current image

    Examples:
     | locale |
     | es_ES  |
     | el_GR  |
     | lt_LT  |
     | gl_ES  |
     | cs_CZ  |
     | ru_RU  |
     | id_ID  |

     # Need a new test for those
     #| hu_HU  |
     #| pl_PL  |
     #| fr_FR  |
     #| sl_SI  |
     #| zh_CN  |
     #| it_IT  |
     #| da_DK  |
     #| de_DE  |
     #| ca_ES  |
     #| sr_RS  |
     #| sr_RS@latin |


     # Error selecting translations
     #| pt_BR  |
     #| zh_TW  |
