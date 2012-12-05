import problem

prob = problem.Runtime(
        reason='Error getting devices:'
        'GDBus.Error:org.freedesktop.DBus.Error.UnknownMethod: '
        'No such interface `org.gnome.SettingsDaemon.Power` on object at path '
        '/org/gnome/SettingsDaemon/Power'
    )

prob.add_current_process_data()
prob.custom_data = 'any'
prob['dict_access_example'] = 'works'

print(prob)
print('')

for key, value in prob.items():
    print('{0}={1}'.format(key, value))

print 'Identifier:', prob.save()
