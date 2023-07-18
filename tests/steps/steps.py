# -*- coding: UTF-8 -*-
from behave import step, then

from common_steps import *
from time import sleep
from dogtail.rawinput import keyCombo
from subprocess import Popen, PIPE
from dogtail import i18n


@step(u'Open About dialog')
def open_about_dialog(context):
    context.execute_steps(u"""
        * Click "About" in GApplication menu
    """)
    context.about_dialog = context.app.dialog(translate('About Eye of GNOME'))


@step(u'Open and close About dialog')
def open_and_close_about_dialog(context):
    context.execute_steps(u'* Click "About" in GApplication menu')
    keyCombo("<Esc>")


@then(u'Website link to wiki is displayed')
def website_link_to_wiki_is_displayed(context):
    assert context.about_dialog.child(translate('Website')).showing


@then(u'GPL 2.0 link is displayed')
def gpl_license_link_is_displayed(context):
    assert context.about_dialog.child(translate("Eye of GNOME")).showing, "App name is not displayed"
    assert context.about_dialog.child(translate("Image viewer for GNOME")).showing, "App description is not displayed"
    assert context.about_dialog.child(translate("Website")).showing, "Website link is not displayed"
    assert context.about_dialog.child(roleName='radio button', name=translate("About")).checked, "About tab is not selected"
    assert not context.about_dialog.child(roleName='radio button', name=translate("Credits")).checked, "Credits tab is selected"


@step(u'Open "{filename}" via menu')
def open_file_via_menu(context, filename):
    keyCombo("<Ctrl>O")
    context.execute_steps(u"""
        * file select dialog with name "Open Image" is displayed
        * In file select dialog select "%s"
    """ % filename)
    sleep(0.5)


@then(u'image size is {width:d}x{height:d}')
def image_size_is(context, width, height):
    size_text = None
    for attempt in range(0, 10):
        size_child = context.app.child(roleName='page tab list').child(translate('Size'))
        size_text = size_child.parent.children[11].text
        if size_text == '':
            sleep(0.5)
            continue
        else:
            break
    try:
        actual_width = size_text.split(' \xc3\x97 ')[0].strip()
        actual_height = size_text.split(' \xc3\x97 ')[1].split(' ')[0].strip()
    except Exception:
        raise Exception("Incorrect width/height is been displayed")
    assert int(actual_width) == width, "Expected width to be '%s', but was '%s'" % (width, actual_width)
    assert int(actual_height) == height, "Expected height to be '%s', but was '%s'" % (height, actual_height)


@step(u'Rotate the image clockwise')
def rotate_image_clockwise(context):
    btn = context.app.child(description=translate('Rotate the image 90 degrees to the left'))
    context.app.child(roleName='drawing area').point()
    sleep(1)
    btn.click()


@step(u'Click Fullscreen button on headerbar')
def click_fullscreen(context):
    context.app.child(translate('Fullscreen')).click()


@step(u'Open context menu for current image')
def open_context_menu(context):
    context.app.child(roleName='drawing area').click(button=3)
    sleep(0.1)


@step(u'Select "{item}" from context menu')
def select_item_from_context_menu(context, item):
    context.app.child(roleName='drawing area').click(button=3)
    sleep(0.1)
    context.app.child(roleName='window').menuItem(item).click()


@then(u'sidepanel is {state:w}')
def sidepanel_displayed(context, state):
    sleep(0.5)
    assert state in ['displayed', 'hidden'], "Incorrect state: %s" % state
    actual = context.app.child(roleName='page tab list').showing
    assert actual == (state == 'displayed')


@then(u'application is {negative:w} fullscreen anymore')
@then(u'application is displayed fullscreen')
def app_displayed_fullscreen(context, negative=None):
    sleep(0.5)
    actual = context.app.child(roleName='drawing area').position[1] == 0
    assert actual == (negative is None)


@step(u'Wait a second')
def wait_a_second(context):
    sleep(1)


@step(u'Click "Hide" in wallpaper popup')
def hide_wallapper_popup(context):
    context.app.button(translate('Hide')).click()


@then(u'wallpaper is set to "{filename}"')
def wallpaper_is_set_to(context, filename):
    wallpaper_path = Popen(["gsettings", "get", "org.gnome.desktop.background", "picture-uri"], stdout=PIPE).stdout.read()
    actual_filename = wallpaper_path.split('/')[-1].split("'")[0]
    assert filename == actual_filename


@then(u'"{filename}" file exists')
def file_exists(context, filename):
    assert os.path.isfile(os.path.expanduser(filename))


@then(u'image type is "{mimetype}"')
def image_type_is(context, mimetype):
    imagetype = context.app.child(roleName='page tab list').child(translate('Type:')).parent.children[-1].text
    assert imagetype == mimetype


@step(u'Select "{menu}" menu')
def select_menuitem(context, menu):
    menu_item = menu.split(' -> ')
    # First level menu
    current = context.app.menu(translate(menu_item[0]))
    current.click()
    if len(menu_item) == 1:
        return
    # Intermediate menus - point them but don't click
    for item in menu_item[1:-1]:
        current = context.app.menu(translate(item))
        current.point()
    # Last level menu item
    current.menuItem(translate(menu_item[-1])).click()


@step(u'Open and close hamburger menu')
def select_hamburger_and_close_it(context):
    context.app.child('Menu').click()
    keyCombo("<Esc>")


@step(u'Select "{name}" window')
def select_name_window(context, name):
    context.app = context.app.child(roleName='frame', name=translate(name))
    context.app.grab_focus()
