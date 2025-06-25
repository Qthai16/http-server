QML component positioning
- Direct positioning: x and y
- anchor, anchor (left, right, top, bottom, horizontal, etc.)
- Row, Column, Grid, RowLayout, ColumnLayout, GridLayout

Javascript integration in QML
- Function list: List of JavaScript Objects and Functions | Qt QML 5.15.14 
- Create component and object instance

Key and mouse event handler
- Flickable component
- Focus scope component

Model, View and Delegate:
- Model: store the data to be displayed, including: ListModel, XMLListModel, JSONListModel, etc
- Delegate: control how data is displayed, can be any QML Component in QT
- View: link between Model and Delegate: ListView, GridView

- Section: grouping data based on properties in Model
- Highlight: highlight current chosen item in ListView
- Header and Footer

- Call C++ method in QML using setContextProperty
- Call C++ method using signal and slot with Connection in QML
- Connect multiple slot with one signal using Connection
- Q_PROPERTY(QString mainCharacter READ getMainCharacter WRITE setMainCharacter NOTIFY mainCharacterChanged)