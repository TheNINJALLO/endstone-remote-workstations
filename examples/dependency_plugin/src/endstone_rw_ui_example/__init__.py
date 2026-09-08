"""Small runnable consumer of RemoteWorkstations; no private implementation access."""
import json

from endstone import Player
from endstone.plugin import Plugin
from endstone_remote_workstations.api import Button, Menu, InventoryMenu, SlotButton, UIError, get_api


class RWUIExample(Plugin):
    api_version = '0.11'
    version = '0.2.0'
    depend = ['remote_workstations']
    commands = {'uidemo': {'description': 'RemoteWorkstations dependency API example',
                          'usages': ['/uidemo [action: string] [type: string]'],
                          'permissions': ['rw_ui_example.use']}}
    permissions = {'rw_ui_example.use': {'description': 'Open the developer API example', 'default': 'op'}}

    def on_enable(self):
        self.ui = get_api(self, minimum=(1, 1))
        self.events = []
        self.ui.register_action('hello', self.hello, permission='rw_ui_example.use', export=True)
        self.ui.register_action('stations', self.stations, permission='rw_ui_example.use')
        self.ui.register_action('ender_status', self.ender_status, permission='rw_ui_example.use')
        self.ui.register_action('submenu', self.submenu, permission='rw_ui_example.use')
        self.ui.register_action('inventory', self.inventory_menu, permission='rw_ui_example.use')
        self.ui.register_action('page2', self.inventory_page_two, permission='rw_ui_example.use')
        self.ui.register_action('ender', lambda ctx: self.ui.open_ender_chest(ctx.player,
            on_open=self.record, on_close=self.record), permission='rw_ui_example.use')
        self.logger.info('RWSDKTEST dependency API acquired; hello exported')

    def record(self, event):
        self.events.append({'kind': event.kind, 'state': event.state, 'reason': event.reason})
        self.events = self.events[-32:]
        self.logger.info('RWSDKTEST lifecycle ' + json.dumps(self.events[-1]))

    def hello(self, context):
        self.events.append({'action': context.action})
        self.events = self.events[-32:]
        self.logger.info('RWSDKTEST action ' + context.action)
        context.player.send_message('Your custom function ran through RemoteWorkstations API 1.0.')
        return 'hello-complete'

    def stations(self, context):
        buttons = tuple(Button(row['id'].title(), 'native:'+row['id'])
                        for row in self.ui.capabilities(context.player) if row['available'] and row['id'] != 'enderchest')
        self.ui.show_menu(context.player, Menu('Native workstations', buttons,
                         'BDS owns item validation, recipes and XP.', 'rw_ui_example.use'),
                         on_open=self.record, on_close=self.record)

    def ender_status(self, context):
        row = next(row for row in self.ui.capabilities(context.player) if row['id'] == 'enderchest')
        context.player.send_message('Ender Chest: ' + (row['unavailable_reason'] or 'Available'))

    def submenu(self, context):
        self.ui.show_menu(context.player, Menu('Custom submenu', (
            Button('Run my function', 'hello'), Button('Open native anvil', 'native:anvil')),
            'Buttons call registered server functions. Add your own plugin logic here.', 'rw_ui_example.use'),
            on_open=self.record, on_close=self.record)

    def inventory_menu(self, context):
        from endstone.inventory import ItemStack
        def icon(kind, name):
            item = ItemStack(kind)
            meta = item.item_meta
            meta.display_name = name
            item.set_item_meta(meta)
            return item
        self.ui.show_inventory(context.player, InventoryMenu('Developer inventory', (
            SlotButton(10, icon('minecraft:diamond', 'Run exported function'), 'hello'),
            SlotButton(13, icon('minecraft:anvil', 'Open real anvil'), 'native:anvil'),
            SlotButton(16, icon('minecraft:ender_chest', 'Open real Ender Chest'), 'ender'),
            SlotButton(22, icon('minecraft:paper', 'Next page'), 'page2'),
        ), 'rw_ui_example.use'), on_open=self.record, on_close=self.record)

    def inventory_page_two(self, context):
        from endstone.inventory import ItemStack
        self.ui.show_inventory(context.player, InventoryMenu('Developer inventory - page 2', (
            SlotButton(11, ItemStack('minecraft:paper'), 'inventory'),
            SlotButton(15, ItemStack('minecraft:diamond'), 'hello'),
        ), 'rw_ui_example.use'), on_open=self.record, on_close=self.record)

    def on_command(self, sender, command, args):
        if not sender.has_permission('rw_ui_example.use'):
            return True
        if args == ['status']:
            sender.send_message('RWSDKTEST status ' + json.dumps(self.events))
            return True
        if not isinstance(sender, Player):
            sender.send_message('Open /uidemo in Minecraft; use /uidemo status from console.')
            return True
        try:
            if args == ['inventory']:
                self.ui.invoke(sender, 'inventory')
            elif args == ['ender']:
                self.ui.open_ender_chest(sender, on_open=self.record, on_close=self.record)
            elif len(args) == 2 and args[0] == 'open':
                self.ui.open_native(sender, args[1], on_open=self.record, on_close=self.record)
            else:
                self.ui.show_menu(sender, Menu('Developer UI demo', (
                    Button('Run exported function', 'hello'),
                    Button('Crafting table', 'native:craft'),
                    Button('Anvil', 'native:anvil'),
                    Button('All available native workstations', 'stations'),
                    Button('Open custom submenu', 'submenu'),
                    Button('Ender Chest status', 'ender_status')),
                    'A separate plugin using the public RemoteWorkstations dependency API.', 'rw_ui_example.use'),
                    on_open=self.record, on_close=self.record)
        except UIError as error:
            sender.send_error_message(str(error))
        return True

    def on_disable(self):
        if hasattr(self, 'ui'):
            self.ui.dispose()
