DELETE FROM `command` WHERE `name` IN ('template apply', 'template disable', 'template enable', 'template list');
INSERT INTO `command` (`name`, `help`) VALUES
('template apply', 'Syntax: .template apply $IndexID\nApplies the given template to your character.'),
('template disable', 'Syntax: .template disable $IndexID\nDisables the given template from being applied.'),
('template enable', 'Syntax: .template enable $IndexID\nEnables the given template to be applied.'),
('template list', 'Syntax: .template list\nList character template sets.');
