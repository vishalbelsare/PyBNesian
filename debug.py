import numpy as np
import pyarrow as pa
import pgm_dataset

from pgm_dataset import LinearGaussianCPD

import pandas as pd

SIZE = 100000
NA_SIZE = 50000
# df = pd.DataFrame({'a': [0.1, np.nan, np.nan], 'b': [-23, np.nan, 4]})

# df = pd.DataFrame({'a': np.random.normal(size=10), 'b': np.random.normal(size=10)})
# df = pd.DataFrame({
#                     'a': pd.Series(np.random.randint(0, 20, size=SIZE), dtype='float'),
#                     'b': pd.Series(np.random.randint(0, 5, size=SIZE), dtype='Int32')
#                     })


a_array = np.random.normal(3, 0.5, size=SIZE)
b_array = 2.5 + 1.65*a_array + np.random.normal(0, 2, size=SIZE)
c_array = -4.2 - 1.2*a_array + 3.2*b_array + np.random.normal(0, 0.75, size=SIZE)


df = pd.DataFrame({
                    'a': a_array,
                    'b': b_array,
                    'c': c_array
                    })



a_nan_indices = np.random.randint(0,SIZE, size=NA_SIZE)
b_nan_indices = np.random.randint(0,SIZE, size=NA_SIZE)
c_nan_indices = np.random.randint(0,SIZE, size=NA_SIZE)

df.loc[a_nan_indices,'a'] = np.nan
df.loc[b_nan_indices,'b'] = np.nan
df.loc[c_nan_indices,'c'] = np.nan


# df.loc[:,'b'] = df.loc[:,'b'].astype('float')
print(df.dtypes)
print(df)
print(df.isna().sum())

df_non_nan = df[["a", "b"]].dropna()
print(df_non_nan)

linregress_data = np.column_stack((np.ones(df_non_nan.shape[0]), df_non_nan[["a"]]))
beta, res,_,_ = np.linalg.lstsq(linregress_data, df_non_nan.loc[:,'b'].values, rcond=None)

print("Python solution: " + str(beta))
print("Python var: " + str(res / (df_non_nan.shape[0]-2)))

df_non_nan = df.dropna()
print(df_non_nan)

linregress_data = np.column_stack((np.ones(df_non_nan.shape[0]), df_non_nan[["a", "b"]]))
beta, res,_,_ = np.linalg.lstsq(linregress_data, df_non_nan.loc[:,'c'].values, rcond=None)

print("Python solution: " + str(beta))
print("Python var: " + str(res / (df_non_nan.shape[0]-3)))


pa_df = pa.RecordBatch.from_pandas(df)

# cpd = LinearGaussianCPD("b", ["a"])
cpd = LinearGaussianCPD("b", ["a"])
cpd.fit(df)
cpd = LinearGaussianCPD("c", ["a", "b"])
cpd.fit(df)
