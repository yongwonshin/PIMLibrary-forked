import pandas as pd
from bokeh.models.widgets import DataTable, TableColumn
from bokeh.io import show, output_file
from bokeh.models import ColumnDataSource, Div
from bokeh.layouts import column

def create_table(df, heading = 'Summary Table', cols = None, width = 1500):
	'''Creates a bokeh Tabular plot
	df = Pandas dataframe contaning data
	heading = Table heading
	cols =  List of columns to display in output. If None, whole dataframe is produced as output
	width = Table width
	'''

	heading_div = Div(text='<br><H1>'+ heading + '</H1>')

    #Filter column from dataframe
	if cols:
		df = df[cols]

	Columns = [TableColumn(field=Ci, title=Ci) for Ci in df.columns] # bokeh columns
	table_plot = DataTable(columns=Columns, source=ColumnDataSource(df), fit_columns = True, width = width) # bokeh table

	return heading_div, table_plot

if __name__ == '__main__':

	#Testing the create_table function with random data
	data = [['kernel1', 120], ['kernel2', 150], ['kernel3', 140]]
	# Create the pandas DataFrame
	df = pd.DataFrame(data, columns = ['Name', 'Average_Time'])

	output_file(filename='testTableViz.html', title='TableViz',mode='inline')
	#call the create_table function to get tabular plot
	heading, table_plot = create_table(df, 'Test Table', ['Name','Average_Time'])
	show(column(heading, table_plot))