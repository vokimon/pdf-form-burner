#include <QtCore/QCoreApplication>
#include <QtCore/QCommandLineParser>
#include <QtCore/QDateTime>
#include <iostream>
#include <poppler-qt5.h>
#include <poppler-form.h>
#include <memory>
#include <fstream>
#include <yaml-cpp/yaml.h>




#define BEGIN_ENUM(NS, TYPE) \
std::ostream & operator << (std::ostream & os, NS::TYPE value) {\
	typedef NS host; \
	switch(value) {
#define ENUM_VALUE(VAL) case(host::VAL): return os << #VAL;
#define END_ENUM \
	default: return os << "Invalid"; \
	}\
}

BEGIN_ENUM(Poppler::FormField, FormType)
	ENUM_VALUE(FormButton)
	ENUM_VALUE(FormText)
	ENUM_VALUE(FormChoice)
	ENUM_VALUE(FormSignature)
END_ENUM

BEGIN_ENUM(Poppler::FormFieldButton, ButtonType)
	ENUM_VALUE(Push)
	ENUM_VALUE(CheckBox)
	ENUM_VALUE(Radio)
END_ENUM

BEGIN_ENUM(Poppler::FormFieldText, TextType)
	ENUM_VALUE(Normal)
	ENUM_VALUE(Multiline)
	ENUM_VALUE(FileSelect)
END_ENUM
  
BEGIN_ENUM(Poppler::SignatureValidationInfo, SignatureStatus)
	ENUM_VALUE(SignatureValid)
	ENUM_VALUE(SignatureInvalid)
	ENUM_VALUE(SignatureDigestMismatch)
	ENUM_VALUE(SignatureDecodingError)
	ENUM_VALUE(SignatureGenericError)
	ENUM_VALUE(SignatureNotFound)
	ENUM_VALUE(SignatureNotVerified)
END_ENUM

QString translate(const char * text) {
	QCoreApplication & app = *QCoreApplication::instance();
	return app.translate("main", text);
}

void dump(Poppler::FormFieldButton * field, YAML::Emitter & out) {
	switch (field->buttonType()) {
		case Poppler::FormFieldButton::CheckBox:
		case Poppler::FormFieldButton::Radio:
			out << bool(field->state());
			return;
		case Poppler::FormFieldButton::Push:
			out << YAML::Null;
			std::cerr
				<< "Push button ignored " << field->fullyQualifiedName().toStdString() << std::endl;
	}
}
void dump(Poppler::FormFieldText * field, YAML::Emitter & out) {
	switch (field->textType()) {
		case Poppler::FormFieldText::FileSelect:
			std::cerr
				<< "Warning: File select not fully supported, managed as simple text"
				<< std::endl;
			out << YAML::Value << field->text().toStdString();
			return;
		case Poppler::FormFieldText::Multiline:
			out << YAML::Literal;
		case Poppler::FormFieldText::Normal:
			out << YAML::Value << field->text().toStdString();
			return;
	}
}

void dump(Poppler::FormFieldChoice * field, YAML::Emitter & out) {
	auto choices = field->choices();
	auto currentChoices = field->currentChoices();
	if (field->multiSelect()) {
		out << YAML::BeginSeq;
		for (auto i: currentChoices) {
			out << YAML::Value << choices[i].toStdString();
		}
		out << YAML::EndSeq;
	}
	else if (field->isEditable() and not field->editChoice().isNull()) {
		out << YAML::Value << field->editChoice().toStdString();
	}
	else {
		out << YAML::Value
			<< (currentChoices.empty()?"":choices[currentChoices.first()].toStdString());
	}
	out << YAML::Comment(translate("%1 values: %2")
		.arg(field->isEditable()?"Suggested":"Allowed")
		.arg(choices.join(", "))
		.toStdString());
}

void dump(Poppler::FormFieldSignature * field, YAML::Emitter & out) {
	auto info = field->validate(
		Poppler::FormFieldSignature::ValidateVerifyCertificate);
	out << YAML::BeginMap;
	out
		<< YAML::Key << "status"
		<< YAML::Value << info.signatureStatus()
		<< YAML::Key << "signer"
		<< YAML::Value << info.signerName().toStdString()
		<< YAML::Key << "time"
		<< YAML::Value << QDateTime::fromMSecsSinceEpoch(
				info.signingTime(), Qt::UTC)
			.toString(Qt::ISODate).toStdString()
		<< YAML::Key << "location"
		<< YAML::Value << info.location().toStdString()
		<< YAML::Key << "reason"
		<< YAML::Value << info.reason().toStdString()
		<< YAML::Key << "scope"
		<< YAML::Value << (info.signsTotalDocument()?"Total":"Partial")
	;
	out << YAML::EndMap;
}


class FieldTree {
public:
	FieldTree(Poppler::FormField * field=nullptr) : _field(field) {}
	void add(const QString & fullName, Poppler::FormField * field) {
		int dotPos = fullName.indexOf('.');
		if (dotPos == -1) {
			_children[fullName] = FieldTree(field);
			return;
		}
		QString levelName = fullName.left(dotPos);
		QString remaining = fullName.mid(dotPos+1);
		if (!_children.contains(levelName)) {
			_children[levelName] = FieldTree();
		}
		FieldTree & level = _children[levelName];
		level.add(remaining, field);
	}
	void extract(YAML::Emitter & out) {
		if (_field) extractField(out);
		else extractChildren(out);
	}

	void extractChildren(YAML::Emitter & out) {
		out << YAML::BeginMap;
		for (auto key : _children.keys()) {
			out << YAML::Key << key.toStdString() << YAML::Value;
			_children[key].extract(out);
		}
		out << YAML::EndMap;
	}

	void extractField(YAML::Emitter & out) {
		switch (_field->type()) {
			case Poppler::FormField::FormButton:
				dump(dynamic_cast<Poppler::FormFieldButton*>(_field), out);
				break;
			case Poppler::FormField::FormText:
				dump(dynamic_cast<Poppler::FormFieldText*>(_field), out);
				break;
			case Poppler::FormField::FormChoice:
				dump(dynamic_cast<Poppler::FormFieldChoice*>(_field), out);
				break;
			case Poppler::FormField::FormSignature:
				dump(dynamic_cast<Poppler::FormFieldSignature*>(_field), out);
				break;
		}
		QString comment = translate("%1%2")
			.arg(_field->name()!=_field->uiName()?_field->uiName():QString())
			.arg(_field->isReadOnly()?" [Read Only]":"")
			;
		if (!comment.isEmpty())
			out << YAML::Comment(comment.toStdString());
	}


	void fill(Poppler::FormField * field, const YAML::Node & node) {
		std::cerr << "Field not supported"
			<< field->fullyQualifiedName().toStdString() << " "
			<< field->type() << " " 
			<< std::endl;
	}

	void fill(Poppler::FormFieldText * field, const YAML::Node & node) {
		if (not node.IsScalar()) {
			std::cerr << "String required for field "
				<< field->fullyQualifiedName().toStdString()
				<< std::endl;
		}
		field->setText(node.as<std::string>().c_str());
	}

	void fill(Poppler::FormFieldButton * field, const YAML::Node & node) {
		switch (field->buttonType()) {
			case Poppler::FormFieldButton::CheckBox:
			case Poppler::FormFieldButton::Radio:
			{
				if (not node.IsScalar()) {
					std::cerr << "Boolean value required for field "
						<< field->fullyQualifiedName().toStdString()
						<< std::endl;
					return;
				}
				bool value = node.as<bool>();
				field->setState(value);
				return;
			}
			case Poppler::FormFieldButton::Push:
				std::cerr
					<< "Push button ignored " << field->fullyQualifiedName().toStdString() << std::endl;
		}
	}

	void fill(Poppler::FormFieldChoice * field, const YAML::Node & node) {
		auto choices = field->choices();
		if (field->multiSelect()) {
			if (not node.IsSequence()) {
				std::cerr << "Sequence required for field "
					<< field->fullyQualifiedName().toStdString()
					<< std::endl;
			}
			QList<int> selection;
			for (auto subnode: node) {
				if (not subnode.IsScalar()) {
					std::cerr << "Sequence of scalars values required for field "
						<< field->fullyQualifiedName().toStdString()
						<< std::endl;
					return;
				}
				std::string value = subnode.as<std::string>();
				int selected = choices.indexOf(value.c_str());
				if (selected==-1) {
					std::cerr << "Illegal value '" << value
						<< "' for field '" << field->fullyQualifiedName().toStdString()
						<< "' try with " << choices.join(", ").toStdString()
						<< std::endl;
					return;
				}
				selection.append(selected);
			}
			field->setCurrentChoices(selection);
			return;
		}
		if (not node.IsScalar()) {
			std::cerr << "Scalar value required for field "
				<< field->fullyQualifiedName().toStdString()
				<< std::endl;
			return;
		}
		std::string value = node.as<std::string>();

		int selected = choices.indexOf(value.c_str());
		if (selected==-1 and field->isEditable()) {
			field->setEditChoice(value.c_str());
			return;
		}
		if (selected==-1) {
			std::cerr << "Illegal value '" << value
				<< "' for field '" << field->fullyQualifiedName().toStdString()
				<< "' try with " << choices.join(", ").toStdString()
				<< std::endl;
			return;
		}
		QList<int> selection;
		selection.append(selected);
		field->setCurrentChoices(selection);
	}

	void fillChildren(const YAML::Node & node) {
		for (auto key : _children.keys()) {
			const YAML::Node & subnode = node[key.toStdString()];
			_children[key].fill(subnode);
		}
	}
	void fillField(const YAML::Node & node) {
		if (!_field) return;
		switch (_field->type()) {
			case Poppler::FormField::FormButton:
				fill(dynamic_cast<Poppler::FormFieldButton*>(_field), node);
				break;
			case Poppler::FormField::FormText:
				fill(dynamic_cast<Poppler::FormFieldText*>(_field), node);
				break;
			case Poppler::FormField::FormChoice:
				fill(dynamic_cast<Poppler::FormFieldChoice*>(_field), node);
				break;
			case Poppler::FormField::FormSignature:
				fill(dynamic_cast<Poppler::FormFieldSignature*>(_field), node);
				break;
		}
	}
	void fill(const YAML::Node & node) {
		if (_field) fillField(node);
		else fillChildren(node);
	}
private:
	QMap<QString, FieldTree> _children;
	Poppler::FormField * _field;
};

int extractYamlFromPdf(FieldTree & fields, std::ostream & outputfile)
{
	YAML::Emitter out(outputfile);
	out << YAML::Comment("Generated by pdf-form-burner");
	fields.extract(out);
	return 0;
}

void fillPdfWithYaml(FieldTree & fields, std::istream & yamlfile)
{
	YAML::Node node = YAML::Load(yamlfile);
	fields.fill(node);
}


int main(int argc, char**argv)
{
	QCoreApplication app(argc, argv);
	app.setApplicationName("pdfformburner");
	app.setOrganizationName("KEEPerians UNLTD");
	app.setOrganizationDomain("kkep.org");
	app.setApplicationVersion("2.0");
	QCommandLineParser parser;
	parser.setApplicationDescription(translate(
		"Extracts and fills PDF form data by means of YAML files"));
	parser.addHelpOption();
	parser.addVersionOption();
	parser.addPositionalArgument("input.pdf",
		translate("PDF file to load"));
	parser.addPositionalArgument("data.yaml",
		translate("YAML file with the form data to write/read. Use a hyphen to use stdin/stdout"));
	parser.addPositionalArgument("output.pdf",
		translate("Filled PDF file. If provided, activates the fill mode and uses the YAML as input"), "[output.pdf]");
	parser.process(app);
	auto arguments = parser.positionalArguments();

	if (arguments.length()<1) {
		parser.showHelp(-1);
	}
	auto inputpdf = arguments[0];

	auto document = std::unique_ptr<Poppler::Document>(Poppler::Document::load(inputpdf));
	if (!document) {
		std::cerr << "Error: Unable to open the document" << std::endl;
		return -1;
	}
	if (document->isLocked()) {
		std::cerr << "Error: Locked pdf" << std::endl;
		return -1;
	}

	FieldTree fieldTree;
	for (unsigned page=0; true; page++) {
		Poppler::Page* pdfPage = document->page(page);  // Document starts at page 0
		if (pdfPage == 0) break;
		auto fields = pdfPage->formFields();
		for (auto field : fields) {
			fieldTree.add(field->fullyQualifiedName(), field);
		}
	}
	switch (arguments.length()) {
		case 1: {
			extractYamlFromPdf(fieldTree, std::cout);
		}
		break;
		case 2: {
			std::ofstream outyaml(arguments[1].toStdString().c_str());
			extractYamlFromPdf(fieldTree, outyaml);
		}
		break;
		case 3: {
			if (arguments[1]=='-') {
				fillPdfWithYaml(fieldTree, std::cin);
			}
			else {
				std::ifstream inyaml(arguments[1].toStdString().c_str());
				fillPdfWithYaml(fieldTree, inyaml);
			}
			std::cerr << "Saving filled pdf as "<< arguments[2].toStdString() << std::endl;
			auto converter = std::unique_ptr<Poppler::PDFConverter>(document->pdfConverter());
			converter->setOutputFileName(arguments[2]);
			converter->setPDFOptions(Poppler::PDFConverter::WithChanges);
			if (!converter->convert()) {
				std::cerr << "Error saving file "<< arguments[2].toStdString() << std::endl;
			}
		}
	}
	return 0;
}

